// VERUTOP: deductive verification for a subset of TOP.
//
// The pipeline, one module per stage; each IR has a printer (Report.h):
//
//   AST, types, ownership            the front end (not here)
//     | Lowering, with Encoding and Specs
//   guarded commands (Gcl)           --pgcl
//     | Passivize
//   passive commands                 --ppassive
//     | VcStrategy: WpVc or PassiveVc
//   a verification condition (Term)  --pvc, --pwp (Outline)
//     | SmtEncode
//   queries (Query)                  --psmt
//     | Solver: Z3Process
//   answers
//     | Feedback, Trust
//   results (Results)                the report, --pcex, --ptrust
//
// This file wires the stages.  It prints nothing and knows nothing about
// Z3 beyond the Solver interface.
//
// Supported: ints and sum types; + - * / and comparisons; if, while (with
// invariants and decreases), case, blocks, assignment, output, error, input;
// alloc, *, & (as a call argument); calls to functions by name, anywhere in
// an expression, verified modularly against the callee's contract; assert
// and assume; spec functions and lemmas.  Function values are not supported.
#include "Driver.h"

#include "Encoding.h"
#include "Feedback.h"
#include "Lowering.h"
#include "Passivize.h"
#include "Report.h"
#include "SmtEncode.h"
#include "Specs.h"
#include "Support.h"
#include "Trust.h"
#include "VcStrategy.h"

#include "AST.h"
#include "TypeInference.h"

#include <map>
#include <memory>
#include <set>

namespace verutop {

VerifyResult verify(ASTProgram *program, TypeInference *types, Solver &solver, const VCGen::Options &opt) {
  VerifyResult res;
  Encoding enc;
  enc.build(program, types);
  Lower low(program, enc, opt.overflow);
  SpecFns specs(program, low);
  std::unique_ptr<VcStrategy> strategy;
  if (opt.passive) strategy = std::make_unique<PassiveVc>();
  else strategy = std::make_unique<WpVc>();
  auto wanted = [&](const std::string &fn) { return opt.fn.empty() || opt.fn == fn; };

  res.rejected = specs.skipped;
  for (auto *f : program->getFunctions()) res.hasEnsures[f->getName()] = !f->getEnsures().empty();

  // What every query shares.  The limit counts Z3's own steps, not
  // seconds, so a verdict depends much less on how busy the machine is.
  Background &bg = res.background;
  bg.rlimit = opt.rlimit;
  bg.sorts = enc.decls;
  bg.functions = specs.decls;
  bg.overflow = opt.overflow;
  // The definitions a query may use: rank functions always; spec functions
  // only if they terminate (step 2).
  bg.axioms = enc.rankAxioms;

  // Queries are independent, so each batch is answered in parallel.  The
  // answers come back in the order asked, and every decision below is made
  // from them in program order: the result does not depend on --vjobs.
  auto askAll = [&](const std::vector<const Query *> &qs) { return checkAll(solver, bg, qs, opt.jobs); };

  // 1. Termination of spec functions, using no spec definitions at all.
  std::set<std::string> unsound, recursive;
  {
    std::vector<const Query *> qs;
    for (auto &c : specs.checks) qs.push_back(&c.query);
    auto answers = askAll(qs);
    for (size_t i = 0; i < specs.checks.size(); ++i) {
      auto &c = specs.checks[i];
      ObligationResult r;
      r.ob = c.ob;
      r.query = c.query;
      r.answer = answers[i];
      if (!r.proved()) unsound.insert(c.fn);
      recursive.insert(c.fn);
      res.specChecks.push_back({c.fn, r});
    }
  }

  // 2. A spec function's definition is used only if it terminates: one
  // that does not, like f(n) = f(n) + 1, can be false, and would prove
  // anything.  (--trust-specs uses it anyway, and the report says so.)
  for (auto &[fn, ax] : specs.axioms) {
    SpecResult s{fn, specs.lhs[fn], specs.rhs[fn], ax};
    s.recursive = recursive.count(fn);
    s.terminates = !unsound.count(fn);
    s.used = s.terminates || opt.trustSpecs;
    if (s.used) bg.axioms[fn] = ax;
    res.specs.push_back(s);
  }

  // 3. The calls between functions (and lemmas), to find recursion through
  // more than one of them.
  res.calls = low.calls();
  Cycles cycles(res.calls);
  low.recursion(cycles);

  // 4. Every function as guarded commands and in passive form, and a
  // query for each of its obligations.
  for (auto *f : program->getFunctions()) {
    if (f->isSpec() || !wanted(f->getName())) continue;
    FunctionResult fr;
    fr.name = f->getName();
    try {
      fr.proc = low.proc(f);
      fr.passive = passivize(fr.proc);
    } catch (Unsupported &u) {
      fr.skipped = u.what();
      res.functions.push_back(fr);
      continue;
    }
    fr.ran = true;
    for (size_t k = 0; k < fr.proc.obs.size(); ++k) {
      ObligationResult r;
      r.ob = fr.proc.obs[k];
      r.index = (int)k;
      r.query = obligationQuery(strategy->vc(fr.proc, fr.passive, (int)k));
      fr.obligations.push_back(r);
    }
    res.functions.push_back(fr);
  }
  // Every obligation result, in order.
  std::vector<std::pair<FunctionResult *, ObligationResult *>> all;
  for (auto &fr : res.functions)
    for (auto &r : fr.obligations) all.push_back({&fr, &r});

  // 5. The verdicts; and rank(x) >= 1 by induction, one step per sum type.
  {
    std::vector<const Query *> qs;
    for (auto &[fr, r] : all)
      if (!r->ob.missing()) qs.push_back(&r->query);
    for (auto &[name, q] : enc.rankChecks) qs.push_back(&q);
    auto answers = askAll(qs);
    size_t i = 0;
    for (auto &[fr, r] : all) {
      // A missing clause fails without asking.
      r->answer = r->ob.missing() ? CannedSolver::sat() : answers[i++];
      if (!r->proved() && !r->ob.missing()) {
        std::vector<std::string> entry;
        for (auto &v : r->query.values) entry.push_back(v->name);
        r->counterexample = counterexample(r->answer, entry);
      }
    }
    for (auto &[name, q] : enc.rankChecks) res.rankProved[name] = answers[i++].verdict == Verdict::Unsat;
  }

  // 6. About each verdict, one more question.
  //
  // A failure (--pcex): a trace.  It comes from the passive program (the
  // obligations are the same in both modes).
  //
  // A proof: does anything reach the obligation?  A proof says nothing if
  // no execution does: an assumption that cannot hold, or a contract or
  // definition that is false, proves everything after it.  So each proved
  // obligation is asked once more, as `false`.  (An obligation that is
  // `false` to begin with claims exactly that the point is unreachable.)
  // Showing that something *is* reachable is not what Z3 is good at when
  // definitions are recursive: it unfolds them until its limit.  A vacuous
  // proof is found quickly or not at all, so this question gets a
  // hundredth of the limit.
  {
    std::vector<Query> traces;
    std::vector<ObligationResult *> traced, reached;
    for (auto &[fr, r] : all) {
      if (opt.pcex && r->answer.verdict == Verdict::Sat) {
        traces.push_back(traceQuery(fr->passive, r->index));
        traced.push_back(r);
      }
      if (r->proved() && !r->ob.unreachable()) {
        Query q = dependencyQuery(fr->proc, fr->passive, r->index, true);
        q.rlimit = opt.rlimit / 100;
        r->asked.push_back({true, q});
        reached.push_back(r);
      }
    }
    std::vector<const Query *> qs;
    for (auto &q : traces) qs.push_back(&q);
    for (auto *r : reached) qs.push_back(&r->asked.back().query);
    auto answers = askAll(qs);
    for (size_t i = 0; i < traced.size(); ++i) {
      auto *r = traced[i];
      const FunctionResult *fr = nullptr;
      for (auto &[f, o] : all)
        if (o == r) fr = f;
      if (answers[i].verdict == Verdict::Sat) r->trace = trace(fr->passive, traces[i], answers[i], r->index);
    }
    std::map<ObligationResult *, Answer> reach;
    for (size_t i = 0; i < reached.size(); ++i) reach[reached[i]] = answers[traced.size() + i];
    for (auto &fr : res.functions) {
      std::set<int> unproved; // among the obligations before this one
      for (auto &r : fr.obligations) {
        auto it = reach.find(&r);
        if (it != reach.end() && it->second.verdict == Verdict::Unsat && vacuous(it->second.core, unproved)) {
          r.vacuous = r.usesKnown = true;
          r.uses = it->second.core;
        }
        if (!r.proved()) unproved.insert(r.index);
      }
    }
  }

  // 7. --ptrust: what does each proof use?  This repeats a proof Z3 has
  // found, with every assumption named.  (For a vacuous proof the answer
  // is already known: what makes the point unreachable.)
  if (opt.ptrust) {
    std::vector<ObligationResult *> asked;
    for (auto &[fr, r] : all)
      if (r->proved() && !r->vacuous) {
        r->asked.push_back({false, dependencyQuery(fr->proc, fr->passive, r->index, false)});
        asked.push_back(r);
      }
    std::vector<const Query *> qs;
    for (auto *r : asked) qs.push_back(&r->asked.back().query);
    auto answers = askAll(qs);
    for (size_t i = 0; i < asked.size(); ++i) {
      asked[i]->usesKnown = answers[i].verdict == Verdict::Unsat;
      asked[i]->uses = answers[i].core;
    }
  }
  return res;
}

} // namespace verutop

bool VCGen::verify(ASTProgram *program, TypeInference *types, const std::string &smtFile,
                   std::ostream &out, const Options &opt) {
  using namespace verutop;
  Z3Process z3;
  VerifyResult res = verutop::verify(program, types, z3, opt);
  TrustReport trust = analyzeTrust(res, opt.trustSpecs);
  long long bytes = writeSmtFile(smtFile, res);
  printReport(out, res, trust, opt, smtFile, bytes, opt.ptrust ? z3.version() : "");
  return res.ok();
}

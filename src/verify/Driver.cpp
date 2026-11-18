// VERUTOP prototype: verification conditions for a subset of TOP.
//
// The pipeline; each stage after the AST can be printed:
//
//   AST  --lower-->      guarded commands              --pgcl
//        --passivize-->  passive commands (versions)   --ppassive
//        --vc-->         one formula per function      --pvc, --pwp (outline)
//        -->             one SMT-LIB query per obligation  --psmt
//        --> Z3: proved, or a counterexample           --pcex (as a trace)
//
// Guarded commands have five forms: assume P, assert P, x := E, havoc x,
// and a choice between two commands, plus sequencing.  Every TOP construct
// the prototype supports lowers to these, so both VC generators (wp by
// substitution, and passive) work on guarded commands, not on the AST.
//
// Heap data needs no heap model, because of ownership:
// - An owned box has one owner, so a variable of type own&T is represented
//   by the value it points to: alloc E, *E, and &E are all the identity, and
//   *p = E assigns p.
// - A sum value is an SMT datatype value; case is a chain of constructor
//   tests, and its binders are selectors.
// - A borrow exists only for the duration of a call, and only an int can be
//   written through one.  So a borrowed parameter is an in-out parameter: the
//   callee's ensures says what *p is at the end (and old(*p) what it was), and
//   the caller's variable takes that value.  A borrowed parameter the callee
//   never writes leaves the caller's variable alone, with no frame condition.
//
// Supported: ints and sum types; + - * / and comparisons; if, while (with
// invariants and decreases), case, blocks, assignment, output, error, input;
// alloc, *, & (as a call argument); calls to functions by name, anywhere in
// an expression, verified modularly against the callee's contract; assert
// and assume; spec functions.  Function values are not supported.
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

#include <memory>
#include <set>

namespace verutop {

VerifyResult verify(ASTProgram *program, TypeInference *types, Solver &solver, const VCGen::Options &opt) {
  VerifyResult res;
  Encoding enc;
  enc.build(program, types);
  Writes writes(program, enc);
  Lower low(program, enc, writes);
  low.overflow = opt.overflow;
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

  // 1. Termination of spec functions, using no spec definitions at all.
  std::set<std::string> unsound, recursive;
  for (auto &c : specs.checks) {
    ObligationResult r;
    r.ob = c.ob;
    r.query = c.query;
    r.answer = solver.check(bg, r.query);
    if (!r.proved()) unsound.insert(c.fn);
    recursive.insert(c.fn);
    res.specChecks.push_back({c.fn, r});
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
  low.edges = &res.calls;
  for (auto *f : program->getFunctions()) {
    if (f->isSpec()) continue;
    try {
      low.proc(f);
    } catch (Unsupported &) {
    }
  }
  low.edges = nullptr;
  Cycles cycles(res.calls);
  low.cycles = &cycles;

  // 4. Every obligation of every function, one query each.
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
    const Proc &proc = fr.proc;
    const Passive &pv = fr.passive;

    std::set<int> unproved;
    for (size_t k = 0; k < proc.obs.size(); ++k) {
      ObligationResult r;
      r.ob = proc.obs[k];
      r.index = (int)k;
      Vc vc = strategy->vc(proc, pv, (int)k);
      r.query = obligationQuery(vc);
      // A missing clause fails without asking.
      r.answer = r.ob.missing() ? CannedSolver::sat() : solver.check(bg, r.query);
      if (!r.proved() && !r.ob.missing()) r.counterexample = counterexample(r.answer, vc.entry);

      // A trace for a failure comes from the passive program (the
      // obligations are the same in both modes).
      if (opt.pcex && r.answer.verdict == Verdict::Sat) {
        Query tq = traceQuery(pv, (int)k);
        Answer ta = solver.check(bg, tq);
        if (ta.verdict == Verdict::Sat) r.trace = trace(pv, tq, ta, (int)k);
      }

      if (r.proved()) {
        // Showing that something *is* reachable is not what Z3 is good at
        // when definitions are recursive: it unfolds them until its limit.
        // A vacuous proof is found quickly or not at all, so that question
        // gets a hundredth of the limit.  The other repeats a proof Z3 has
        // found.
        auto ask = [&](bool reach) {
          Query dq = dependencyQuery(proc, pv, (int)k, reach);
          if (reach) dq.rlimit = opt.rlimit / 100;
          r.asked.push_back({reach, dq});
          return solver.check(bg, dq);
        };
        // A proof says nothing if no execution reaches the obligation: an
        // assumption that cannot hold, or a contract or definition that is
        // false, proves everything after it.  So each proved obligation is
        // asked once more, as `false`.  (An obligation that is `false` to
        // begin with claims exactly that the point is unreachable.)
        if (!r.ob.unreachable()) {
          Answer reach = ask(true);
          if (reach.verdict == Verdict::Unsat && vacuous(reach.core, unproved)) {
            r.vacuous = r.usesKnown = true;
            r.uses = reach.core;
          }
        }
        if (!r.vacuous && opt.ptrust) {
          Answer uses = ask(false);
          r.usesKnown = uses.verdict == Verdict::Unsat;
          r.uses = uses.core;
        }
      } else {
        unproved.insert((int)k);
      }
      fr.obligations.push_back(r);
    }
    res.functions.push_back(fr);
  }

  // 5. rank(x) >= 1, by induction: one step per sum type.
  for (auto &[r, q] : enc.rankChecks) res.rankProved[r] = solver.check(bg, q).verdict == Verdict::Unsat;
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

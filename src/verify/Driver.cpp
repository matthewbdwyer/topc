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
#include "VCGen.h"

#include "Encoding.h"
#include "Feedback.h"
#include "Gcl.h"
#include "Lowering.h"
#include "Obligations.h"
#include "Outline.h"
#include "Passivize.h"
#include "Query.h"
#include "SmtEncode.h"
#include "Specs.h"
#include "Support.h"
#include "Term.h"
#include "TermPrint.h"
#include "Termination.h"
#include "VcStrategy.h"

#include "AST.h"
#include "ASTCtorPattern.h"
#include "ASTOldExpr.h"
#include "ASTProofStmt.h"
#include "ASTRankExpr.h"
#include "ASTVarPattern.h"
#include "ASTVisitor.h"
#include "ASTWildcardPattern.h"
#include "ReferenceMode.h"
#include "ReferenceType.h"
#include "SemanticError.h"
#include "TopFunction.h"
#include "TopInt.h"
#include "TopMu.h"
#include "TopSumType.h"
#include "TypeInference.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <memory>
#include <tuple>

using namespace verutop;

namespace {
void heading(std::ostream &out, const std::string &h) { out << "-- " << h << "\n"; }
} // namespace

bool VCGen::verify(ASTProgram *program, TypeInference *types, const std::string &smtFile,
                   std::ostream &out, const Options &opt) {
  Encoding enc;
  enc.build(program, types);
  Writes writes(program, enc);
  Lower low(program, enc, writes);
  SpecFns specs(program, low);
  std::unique_ptr<VcStrategy> strategy;
  if (opt.passive) strategy = std::make_unique<PassiveVc>();
  else strategy = std::make_unique<WpVc>();
  for (auto &sk : specs.skipped) out << sk << " (spec function rejected)\n";

  struct Row {
    std::string fn;
    Obligation ob;
    Query query;
    std::string verdict, model;
    std::vector<std::string> trace;
    int order = 0;                 // of its function, in the report
    bool limited = false;          // unknown because Z3 reached the resource limit
    bool vacuous = false;          // proved, but no execution reaches it
    bool usesKnown = false;        // Z3 said what the proof depends on
    std::vector<std::string> uses; // names from the unsat core: asm!k, ob!j, def!f, rank!T
  };
  // What the report needs to know about each function.
  struct FnInfo {
    bool ran = false;
    std::string skipped;
    std::vector<Assumption> assumptions;
    std::vector<Obligation> obs;
    std::vector<std::string> diverges;
    int arithmetic = 0;
  };
  std::map<std::string, FnInfo> info;
  std::vector<std::string> fnOrder;

  // Run one query in its own Z3 process.  (In one process, state left by an
  // earlier query, even after push/pop, can make Z3 give up on a later one.)
  // Returns Z3's answer and whatever it printed after it.  Z3 is also asked
  // why, in case the answer is `unknown`: `limited` is set when it stopped at
  // the resource limit and not because the logic is undecidable.  (The
  // question is added here, so --psmt and the .vc.smt2 file do not show it.)
  bool limited = false;
  auto run = [&](const std::string &text) -> std::pair<std::string, std::string> {
    std::string one = smtFile + ".one";
    std::string full = text;
    const std::string check = "(check-sat)\n";
    if (auto at = full.find(check); at != std::string::npos)
      full.insert(at + check.size(), "(get-info :reason-unknown)\n");
    {
      std::ofstream f(one);
      f << full;
    }
    FILE *pipe = popen(("z3 " + one + " 2>&1").c_str(), "r");
    std::vector<std::string> lines;
    char buf[4096];
    while (pipe && fgets(buf, sizeof buf, pipe)) {
      std::string l(buf);
      if (!l.empty() && l.back() == '\n') l.pop_back();
      lines.push_back(l);
    }
    if (pipe) pclose(pipe);
    std::remove(one.c_str());
    limited = false;
    if (lines.size() > 1 && lines[1].rfind("(:reason-unknown", 0) == 0) {
      limited = lines[0] == "unknown" && (lines[1].find("canceled") != std::string::npos ||
                                          lines[1].find("resource limit") != std::string::npos);
      lines.erase(lines.begin() + 1);
    }
    std::string rest;
    for (size_t i = 1; i < lines.size(); ++i) rest += lines[i] + "\n";
    return {lines.empty() ? "no answer" : lines[0], rest};
  };
  auto oneLine = [](std::string s) {
    s.erase(std::remove(s.begin(), s.end(), '\n'), s.end());
    return s;
  };
  auto wanted = [&](const std::string &fn) { return opt.fn.empty() || opt.fn == fn; };

  // What every query shares.  The limit counts Z3's own steps, not
  // seconds, so a verdict depends much less on how busy the machine is.
  Background bg;
  bg.rlimit = opt.rlimit;
  bg.sorts = enc.decls;
  bg.functions = specs.decls;
  bg.overflow = opt.overflow;
  // Definitions a query may use: rank functions always, spec functions
  // only if they terminate (step 2).
  bg.axioms = enc.rankAxioms;
  low.overflow = opt.overflow;

  // ---- spec functions -----------------------------------------------------
  if (opt.pspec) {
    for (auto &[fn, l] : specs.lhs) {
      if (!wanted(fn) && !wanted("spec " + fn)) continue;
      out << "== spec " << fn << "\n";
      heading(out, "definition (an axiom, triggered on each application)");
      auto L = pretty(specs.rhs[fn], 66);
      out << "  " << flat(l) << " ==\n";
      for (auto &x : L) out << "    " << x << "\n";
      out << "  " << render(specs.axioms.at(fn));
      bool any = false;
      for (auto &c : specs.checks)
        if (c.fn == fn) {
          if (!any) heading(out, "termination");
          any = true;
          out << "  line " << c.ob.line << ": " << c.ob.what() << "\n";
          for (auto &x : pretty(c.query.goal, 70)) out << "    " << x << "\n";
        }
      out << "\n";
    }
  }

  // Ask what the proof of obligation k depends on; with `reach`, whether
  // anything reaches the obligation at all.  Returns whether Z3 proved the
  // query, and the names in its unsat core.
  //
  // Showing that something *is* reachable is not what Z3 is good at when
  // definitions are recursive: it unfolds them until its limit.  A vacuous
  // proof is found quickly or not at all, so the reachability question
  // gets a hundredth of the limit.  The dependency question repeats a
  // proof Z3 has found.
  auto dependencies = [&](const Proc &proc, const Passive &pv, int k, bool reach) {
    Query dq = dependencyQuery(proc, pv, k, reach);
    if (reach) dq.rlimit = opt.rlimit / 100;
    std::string full = render(bg, dq);
    // --psmt with --ptrust shows these queries too.
    if (opt.psmt && opt.ptrust && (opt.psmtLine == 0 || opt.psmtLine == proc.obs[k].line))
      out << "; ==== " << proc.fn->getName() << " #" << k << ", line " << proc.obs[k].line << ": "
          << proc.obs[k].what() << (reach ? " -- does anything reach it?" : " -- what does the proof use?")
          << "\n" << full << "\n";
    auto [v, rest] = run(full);
    std::vector<std::string> names;
    if (v == "unsat") {
      std::string name;
      for (char c : rest + " ") {
        if (c == '(' || c == ')' || isspace((unsigned char)c)) {
          if (!name.empty()) names.push_back(name);
          name.clear();
        } else {
          name += c;
        }
      }
    }
    return std::make_pair(v == "unsat", names);
  };

  // 1. Termination of spec functions, using no spec definitions at all.
  std::vector<Row> queries;
  std::set<std::string> unsound;
  for (auto &c : specs.checks) {
    Row q{"spec " + c.fn, c.ob, c.query};
    auto [v, rest] = run(render(bg, q.query));
    q.verdict = v;
    q.limited = limited;
    if (v != "unsat") unsound.insert(c.fn);
    queries.push_back(q);
  }

  // 2. A spec function's definition is used only if it terminates.
  std::string defs;
  for (auto &[fn, ax] : specs.axioms) {
    if (unsound.count(fn) && !opt.trustSpecs) {
      out << "spec " << fn << ": does not terminate; its definition is not used\n";
      continue;
    }
    if (unsound.count(fn))
      out << "spec " << fn << ": does not terminate, but --trust-specs uses its definition\n";
    bg.axioms[fn] = ax;
    defs += render(ax);
  }

  // 3. The calls between functions (and lemmas), to find recursion through
  // more than one of them.
  CallEdges edges;
  low.edges = &edges;
  for (auto *f : program->getFunctions()) {
    if (f->isSpec()) continue;
    try {
      low.proc(f);
    } catch (Unsupported &) {
    }
  }
  low.edges = nullptr;
  Cycles cycles(edges);
  low.cycles = &cycles;

  // 4. Every obligation of every function, one query each.
  for (auto *f : program->getFunctions()) {
    if (f->isSpec() || !wanted(f->getName())) continue;
    Proc proc;
    Passive pv;
    try {
      proc = low.proc(f);
      pv = passivize(proc);
    } catch (Unsupported &u) {
      out << f->getName() << ": skipped (" << u.what() << ")\n";
      info[f->getName()].skipped = u.what();
      continue;
    }
    fnOrder.push_back(f->getName());
    info[f->getName()] = {true, "", proc.assumptions, proc.obs, proc.diverges, proc.arithmetic};

    std::string sig = f->getName() + "(";
    for (size_t i = 0; i < f->getFormals().size(); ++i)
      sig += (i ? ", " : "") + f->getFormals()[i]->getName();
    sig += ")";
    bool dumps = opt.pgcl || opt.ppassive || opt.pwp || opt.pvc;
    if (dumps) out << "== " << sig << "\n";
    if (opt.pgcl) {
      heading(out, "guarded commands");
      printG(proc.body, 2, out, false);
    }
    if (opt.ppassive) {
      heading(out, "passive commands: entry versions " + join(pv.entry));
      printG(pv.prog, 2, out, true);
    }
    if (opt.pwp) {
      heading(out, "proof outline: the weakest precondition before each command");
      auto o = outline(proc.body, TRUE);
      for (auto &l : o.lines) out << "  " << l << "\n";
      for (auto &l : braces(TRUE)) out << "  " << l << "\n";
    }
    if (opt.pvc) {
      if (opt.passive) {
        auto vc = passiveVc(pv.prog, -2);
        heading(out, "VC (passive), every obligation checked: VC must be valid");
        for (auto &[n, t] : vc.defs) {
          auto L = pretty(t, 66);
          for (size_t i = 0; i < L.size(); ++i)
            out << "  " << (i ? pad(n.size() + 4) : n + " := ") << L[i] << "\n";
        }
        auto L = pretty(vc.ok, 66);
        for (size_t i = 0; i < L.size(); ++i) out << "  " << (i ? "    " : "VC: ") << L[i] << "\n";
      } else {
        heading(out, "VC (wp), every obligation checked: it must be valid");
        for (auto &l : pretty(wpFunction(proc, -2), 74)) out << "  " << l << "\n";
      }
    }
    if (dumps) out << "\n";

    std::set<int> unprovedObs;
    for (size_t k = 0; k < proc.obs.size(); ++k) {
      Vc vc = strategy->vc(proc, pv, (int)k);
      const std::vector<std::string> &model = vc.entry;
      Row q{f->getName(), proc.obs[k], obligationQuery(vc)};
      q.order = (int)fnOrder.size();
      std::string full = render(bg, q.query);
      if (opt.psmt && (opt.psmtLine == 0 || opt.psmtLine == q.ob.line))
        out << "; ==== " << f->getName() << " #" << k << ", line " << q.ob.line << ": "
            << q.ob.what() << "\n"
            << full << "\n";
      std::string v = "sat", rest;
      if (!q.ob.missing()) std::tie(v, rest) = run(full);
      q.verdict = v;
      q.limited = !q.ob.missing() && limited;
      if (v != "unsat" && !q.ob.missing()) {
        size_t i = 0;
        SX vals = parseSX(rest, i);
        std::vector<std::string> shown;
        for (size_t j = 0; j < vals.kids.size() && j < model.size(); ++j)
          if (vals.kids[j].kids.size() == 2)
            shown.push_back(baseName(model[j]) + " = " + valueOf(vals.kids[j].kids[1]));
        q.model = shown.empty() ? oneLine(rest) : join(shown);
      }

      // A trace for a failure comes from the passive query (the obligations
      // are the same in both modes), asked for every version and every
      // branch condition.
      if (opt.pcex && v == "sat") {
        Query tq = traceQuery(pv, (int)k);
        std::vector<std::string> asked;
        for (auto &t : tq.values) asked.push_back(smt(t));
        auto [tv, trest] = run(render(bg, tq));
        if (tv == "sat") {
          size_t i = 0;
          SX vals = parseSX(trest, i);
          std::map<std::string, std::string> val;
          for (size_t j = 0; j < vals.kids.size() && j < asked.size(); ++j)
            if (vals.kids[j].kids.size() == 2) val[asked[j]] = valueOf(vals.kids[j].kids[1]);
          Trace tr(val, (int)k);
          std::vector<std::string> vs;
          for (auto &e : pv.entry) vs.push_back(baseName(e) + " = " + val["|" + e + "|"]);
          if (!vs.empty()) tr.lines.push_back("    entry     " + join(vs));
          tr.walk(pv.prog);
          q.trace = tr.lines;
        }
      }
      // A proof says nothing if no execution reaches the obligation: an
      // assumption that cannot hold, or a contract or definition that is
      // false, proves everything after it.  So each proved obligation is
      // asked once more, as `false`.  (An obligation that is `false` to
      // begin with claims exactly that the point is unreachable.)
      if (v == "unsat" && !q.ob.unreachable()) {
        auto [unreached, why] = dependencies(proc, pv, (int)k, true);
        // Unreachable only because an obligation that failed is assumed
        // after it: the failure is already reported.
        for (auto &n : why)
          if (n.rfind("ob!", 0) == 0 && unprovedObs.count(std::atoi(n.c_str() + 3))) unreached = false;
        if (unreached) {
          q.vacuous = q.usesKnown = true;
          q.uses = why;
        } else if (opt.ptrust) {
          std::tie(q.usesKnown, q.uses) = dependencies(proc, pv, (int)k, false);
        }
      } else if (v == "unsat" && opt.ptrust) {
        std::tie(q.usesKnown, q.uses) = dependencies(proc, pv, (int)k, false);
      }
      if (v != "unsat") unprovedObs.insert((int)k);
      queries.push_back(q);
    }
  }

  // rank(x) >= 1, by induction: one step per sum type.
  std::map<std::string, bool> rankProved;
  for (auto &[r, q] : enc.rankChecks) rankProved[r] = run(render(bg, q)).first == "unsat";

  // The same queries as one readable SMT-LIB file.
  std::ofstream smtOut(smtFile);
  smtOut << renderPrelude(bg, bg.rlimit);
  for (auto &[r, ax] : enc.rankAxioms) smtOut << render(ax);
  for (auto &q : queries)
    if (q.fn.rfind("spec ", 0) == 0)
      smtOut << "\n; " << q.fn << ", line " << q.ob.line << ": " << q.ob.what() << "\n(push)\n"
             << renderBody(q.query) << "(pop)\n";
  smtOut << "\n; definitions of the spec functions that terminate\n" << defs;
  for (auto &q : queries)
    if (q.fn.rfind("spec ", 0) != 0)
      smtOut << "\n; " << q.fn << ", line " << q.ob.line << ": " << q.ob.what() << "\n(push)\n"
             << renderBody(q.query) << "(pop)\n";
  auto bytes = smtOut.tellp();
  smtOut.close();
  out << "VC (" << (opt.passive ? "passive" : "wp") << "): " << queries.size() << " queries, "
      << bytes << " bytes in " << smtFile << "\n";

  std::stable_sort(queries.begin(), queries.end(), [](auto &x, auto &y) {
    return std::tie(x.order, x.ob.line) < std::tie(y.order, y.ob.line);
  });
  bool allOk = true;
  std::string fn;
  for (auto &r : queries) {
    bool ok = r.verdict == "unsat";
    allOk = allOk && ok;
    if (r.fn.rfind("spec ", 0) == 0 && !wanted(r.fn.substr(5)) && !wanted(r.fn)) continue;
    if (r.fn != fn) {
      fn = r.fn;
      out << fn << ":\n";
    }
    out << "  " << (ok ? "proved " : (r.verdict == "sat" ? "FAILED " : "UNKNOWN")) << " line "
        << r.ob.line << ": " << r.ob.what();
    if (!ok && r.trace.empty() && !r.model.empty() && r.model.find("error") == std::string::npos)
      out << "   when " << r.model;
    if (r.verdict != "sat" && r.verdict != "unsat" && r.verdict != "unknown")
      out << "   (z3: " << r.verdict << ")";
    if (r.vacuous) out << "   VACUOUS";
    out << "\n";
    for (auto &l : r.trace) out << l << "\n";
  }
  // An UNKNOWN that more work might settle says so, and says how to allow it.
  std::vector<std::string> atLimit;
  for (auto &r : queries)
    if (r.limited) atLimit.push_back(r.fn + " line " + std::to_string(r.ob.line));
  if (!atLimit.empty())
    out << "Z3 reached its resource limit (" << opt.rlimit << ") on: " << join(atLimit)
        << ". A larger limit may decide " << (atLimit.size() > 1 ? "them" : "it")
        << ": --vrlimit=N.\n";

  // ---- what the proofs rest on ----------------------------------------------
  auto verified = [&](const std::string &fn) {
    if (!info.count(fn) || !info[fn].ran) return false;
    for (auto &q : queries)
      if (q.fn == fn && q.verdict != "unsat") return false;
    return true;
  };
  auto whyNot = [&](const std::string &fn) -> std::string {
    if (!info.count(fn)) return "not checked in this run";
    if (!info[fn].skipped.empty()) return "not checked: " + info[fn].skipped;
    return "not verified";
  };
  // A name from an unsat core, as text.
  auto show = [&](const std::string &fn, const std::string &name) -> std::string {
    auto n = name.substr(name.find('!') + 1);
    if (name.rfind("def!", 0) == 0) return "definition of " + n;
    if (name.rfind("rank!", 0) == 0) return "rank(" + n + ")";
    size_t i = (size_t)std::atoi(n.c_str());
    if (name.rfind("asm!", 0) == 0 && i < info[fn].assumptions.size()) return info[fn].assumptions[i].what;
    if (name.rfind("ob!", 0) == 0 && i < info[fn].obs.size())
      return "the obligation at line " + std::to_string(info[fn].obs[i].line) + " (" +
             info[fn].obs[i].what() + ")";
    return name;
  };

  // Assumed without proof, wherever a proof could use it.
  std::vector<std::string> assumed, checked;
  for (auto &fn : fnOrder)
    for (auto &a : info[fn].assumptions)
      if (a.kind == Assumption::Assume) assumed.push_back(a.what + " in " + fn);
  for (auto &[fn, ax] : specs.axioms)
    if (unsound.count(fn) && opt.trustSpecs)
      assumed.push_back("definition of " + fn + ": does not terminate (--trust-specs)");
  // A contract is used by every caller, verified or not.
  std::map<std::string, std::set<std::string>> callers;
  for (auto &[f, g] : edges)
    if (f != g && info.count(f) && info[f].ran && !verified(g) && !low.fns[g]->getEnsures().empty())
      callers[g].insert(f);
  for (auto &[g, fs] : callers)
    assumed.push_back("contract of " + g + " (" + whyNot(g) + "), used by " +
                      join(std::vector<std::string>(fs.begin(), fs.end())));
  int arithmetic = 0;
  for (auto &fn : fnOrder) arithmetic += info[fn].arithmetic;
  if (arithmetic)
    assumed.push_back(std::to_string(arithmetic) + " arithmetic operation" + (arithmetic > 1 ? "s" : "") +
                      " in code: no overflow (--overflow checks " + (arithmetic > 1 ? "them" : "it") + ")");
  for (auto &[r, ok] : rankProved)
    if (!ok) assumed.push_back("rank(" + r.substr(5) + ") >= 1");

  for (auto &[fn, ax] : specs.axioms) {
    if (unsound.count(fn)) continue;
    bool recursive = false;
    for (auto &c : specs.checks) recursive = recursive || c.fn == fn;
    checked.push_back("definition of " + fn + (recursive ? ": terminates" : ": not recursive"));
  }
  for (auto &[r, ok] : rankProved)
    if (ok) checked.push_back("rank(" + r.substr(5) + ") >= 1: induction step");
  std::vector<std::string> diverges;
  for (auto &fn : fnOrder)
    for (auto &d : info[fn].diverges) diverges.push_back(d == fn ? fn : d + " in " + fn);
  bool anyVacuous = false;
  for (auto &q : queries) anyVacuous = anyVacuous || q.vacuous;

  if (opt.ptrust) {
    // What each proof used, and what each function's verdict rests on.
    out << "\nTrust report\n";
    std::map<std::string, std::set<std::string>> rests; // function -> unproved things it rests on
    std::map<std::string, std::set<std::string>> usedCallees;
    std::map<std::string, std::vector<std::string>> lines;
    for (auto &q : queries) {
      if (q.verdict != "unsat" || !info.count(q.fn)) continue;
      std::string head = "  line " + std::to_string(q.ob.line) + ": " + q.ob.what();
      if (!q.usesKnown) {
        lines[q.fn].push_back(head);
        lines[q.fn].push_back("      Z3 did not say what this proof depends on");
        rests[q.fn].insert("whatever the proof at line " + std::to_string(q.ob.line) + " depends on");
        continue;
      }
      std::vector<std::string> shown;
      for (auto &u : q.uses) {
        size_t i = (size_t)std::atoi(u.substr(u.find('!') + 1).c_str());
        bool isAsm = u.rfind("asm!", 0) == 0 && i < info[q.fn].assumptions.size();
        auto kind = isAsm ? info[q.fn].assumptions[i].kind : Assumption::Pre;
        // The precondition and the function's own proved obligations are
        // not news, unless they explain why nothing reaches a point.
        bool ownProved = u.rfind("ob!", 0) == 0;
        if (ownProved)
          for (auto &o : queries)
            if (o.fn == q.fn && i < info[q.fn].obs.size() && o.ob.line == info[q.fn].obs[i].line &&
                o.ob.what() == info[q.fn].obs[i].what() && o.verdict != "unsat")
              ownProved = false;
        if (!q.vacuous && ((isAsm && kind == Assumption::Pre) || ownProved)) continue;
        shown.push_back(show(q.fn, u));
        if (isAsm && kind == Assumption::Assume) rests[q.fn].insert(info[q.fn].assumptions[i].what);
        if (isAsm && (kind == Assumption::Contract || kind == Assumption::Lemma))
          usedCallees[q.fn].insert(info[q.fn].assumptions[i].callee);
        if (u.rfind("def!", 0) == 0 && unsound.count(u.substr(4)))
          rests[q.fn].insert("definition of " + u.substr(4) + " (--trust-specs)");
      }
      if (q.vacuous) {
        lines[q.fn].push_back(head);
        lines[q.fn].push_back("      VACUOUS: nothing reaches line " + std::to_string(q.ob.line) +
                              (shown.empty() ? ", whatever is assumed" : ", given " + join(shown)));
      } else if (!shown.empty()) {
        lines[q.fn].push_back(head);
        lines[q.fn].push_back("      uses " + join(shown));
      }
    }
    // A function rests on what its callees rest on.
    for (auto &[f, gs] : usedCallees)
      for (auto &g : gs)
        if (g != f && !verified(g)) rests[f].insert("contract of " + g + " (" + whyNot(g) + ")");
    for (bool changed = true; changed;) {
      changed = false;
      for (auto &[f, gs] : usedCallees)
        for (auto &g : gs) {
          if (g == f || !verified(g)) continue;
          for (auto &r : rests[g]) {
            auto via = r.find(" (through ") == std::string::npos ? r + " (through " + g + ")" : r;
            changed = rests[f].insert(via).second || changed;
          }
        }
    }
    for (auto &fn : fnOrder) {
      out << fn << ":\n";
      for (auto &l : lines[fn]) out << l << "\n";
      int total = 0, unproved = 0, vacuous = 0;
      for (auto &q : queries)
        if (q.fn == fn) {
          ++total;
          unproved += q.verdict != "unsat";
          vacuous += q.vacuous;
        }
      std::string v;
      if (unproved)
        v = "not verified: " + std::to_string(unproved) + " of " + std::to_string(total) +
            (total > 1 ? " obligations" : " obligation") + " not proved";
      else if (rests[fn].empty())
        v = "verified";
      else
        v = "verified, assuming: " + join(std::vector<std::string>(rests[fn].begin(), rests[fn].end()), "; ");
      if (vacuous)
        v += "; " + std::to_string(vacuous) + (vacuous > 1 ? " proofs are" : " proof is") + " vacuous";
      out << "  => " << v << "\n";
    }
  }

  auto list = [&](const std::string &title, const std::vector<std::string> &items) {
    if (items.empty()) return;
    out << title << ":\n";
    for (auto &i : items) out << "  " << i << "\n";
  };
  if (opt.ptrust || !assumed.empty() || !checked.empty() || !diverges.empty() || anyVacuous) out << "\n";
  list("Assumed without proof", assumed);
  list("Not shown to terminate (decreases *)", diverges);
  list("Checked in this run", checked);
  if (anyVacuous && !opt.ptrust)
    out << "VACUOUS: no execution reaches the obligation, so its proof says nothing "
           "(--ptrust says why)\n";
  if (opt.ptrust) {
    std::string z3 = "Z3";
    if (FILE *p = popen("z3 --version 2>/dev/null", "r")) {
      char buf[256];
      if (fgets(buf, sizeof buf, p)) {
        z3 = buf;
        z3 = z3.substr(0, z3.find(" - "));
        while (!z3.empty() && isspace((unsigned char)z3.back())) z3.pop_back();
      }
      pclose(p);
    }
    list("Always trusted",
         {"topc's parser, type checker, and ownership checks (the encoding relies on ownership)",
          "VERUTOP's translation of the program to verification conditions",
          z3,
          "erasure, code generation, LLVM, and the runtime: the source is verified, not the binary",
          "input is any integer; output is not modelled"});
  }
  return allOk;
}


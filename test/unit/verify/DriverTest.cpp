#include "ASTHelper.h"
#include "Driver.h"
#include "Report.h"
#include "SemanticAnalysis.h"
#include "TermPrint.h"

#include <catch2/catch_test_macros.hpp>

#include <sstream>

using namespace verutop;

namespace {
// A checked program, and the verifier run on it with a solver that
// answers by rule.  No Z3 is involved.
struct Run {
  std::shared_ptr<ASTProgram> ast;
  std::shared_ptr<SemanticAnalysis> sem;
  VCGen::Options opt;
  VerifyResult res;

  Run(const std::string &source, CannedSolver &solver, VCGen::Options o = {}) : opt(o) {
    std::stringstream stream(source);
    ast = ASTHelper::build_ast(stream);
    sem = SemanticAnalysis::analyze(ast.get());
    res = verify(ast.get(), sem->getTypeResults(), solver, opt);
  }
  std::string report() const {
    std::ostringstream out;
    printReport(out, res, analyzeTrust(res, opt.trustSpecs), opt, "p.top.vc.smt2", 0, "a canned solver");
    return out.str();
  }
};

bool isReach(const Query &q) { return q.core && q.rlimit != 0; } // "does anything reach it?"
bool isUses(const Query &q) { return q.core && q.rlimit == 0; }  // "what does the proof use?"
bool isVerdict(const Query &q) { return !q.core; }

// Proves everything, and finds every obligation reachable.
Answer agreeable(const Query &q) { return isReach(q) ? CannedSolver::sat() : CannedSolver::unsat(); }

const char *ABS = R"(
abs(x)
  ensures result >= 0
{
  var r;
  if (x < 0) { r = 0 - x; } else { r = x; }
  return r;
}
)";
} // namespace

TEST_CASE("one query per obligation, and one more asking whether it is reachable", "[verify][Driver]") {
  CannedSolver solver(agreeable);
  Run run(ABS, solver);
  REQUIRE(run.res.functions.size() == 1);
  auto &f = run.res.functions[0];
  REQUIRE(f.ran);
  REQUIRE(f.obligations.size() == 1);
  REQUIRE(f.obligations[0].ob.kind == ObKind::Postcondition);
  REQUIRE(f.obligations[0].proved());
  REQUIRE_FALSE(f.obligations[0].vacuous);
  REQUIRE(run.res.ok());
  REQUIRE(solver.asked.size() == 2);
  REQUIRE(isVerdict(solver.asked[0]));
  REQUIRE(isReach(solver.asked[1]));
  // The reachability question gets a hundredth of the limit.
  REQUIRE(solver.asked[1].rlimit == run.opt.rlimit / 100);
}

TEST_CASE("--ptrust also asks what each proof uses", "[verify][Driver]") {
  CannedSolver solver(agreeable);
  VCGen::Options opt;
  opt.ptrust = true;
  Run run(ABS, solver, opt);
  REQUIRE(solver.asked.size() == 3);
  REQUIRE(isUses(solver.asked[2]));
  REQUIRE(run.res.functions[0].obligations[0].usesKnown);
  REQUIRE(run.res.functions[0].obligations[0].asked.size() == 2);
}

TEST_CASE("a counterexample names the arguments", "[verify][Driver]") {
  CannedSolver solver([](const Query &) { return CannedSolver::sat({"-3"}); });
  Run run(ABS, solver);
  auto &r = run.res.functions[0].obligations[0];
  REQUIRE(r.answer.verdict == Verdict::Sat);
  REQUIRE(r.counterexample == "x = -3");
  REQUIRE_FALSE(run.res.ok());
  REQUIRE(solver.asked.size() == 1); // nothing more is asked about a failure
  REQUIRE(run.report().find("  FAILED  line 7: postcondition   when x = -3\n") != std::string::npos);
}

TEST_CASE("both strategies ask about the same obligation", "[verify][Driver]") {
  CannedSolver passive(agreeable), wp(agreeable);
  VCGen::Options o;
  o.passive = false;
  Run a(ABS, passive), b(ABS, wp, o);
  // Passive declares every version and names each post; wp declares the formal.
  REQUIRE(passive.asked[0].consts.size() > 1);
  REQUIRE(!passive.asked[0].defs.empty());
  REQUIRE(wp.asked[0].consts == std::vector<Decl>{{"x", "Int"}});
  REQUIRE(wp.asked[0].defs.empty());
  REQUIRE(a.res.functions[0].obligations[0].ob.what() == b.res.functions[0].obligations[0].ob.what());
}

TEST_CASE("an unknown caused by the limit is reported with the flag to raise it", "[verify][Driver]") {
  CannedSolver solver([](const Query &) { return CannedSolver::unknown(true); });
  Run run(ABS, solver);
  REQUIRE(run.res.functions[0].obligations[0].answer.limited);
  std::string report = run.report();
  REQUIRE(report.find("  UNKNOWN line 7: postcondition\n") != std::string::npos);
  REQUIRE(report.find("Z3 reached its resource limit (20000000) on: abs line 7. A larger limit may decide it: "
                      "--vrlimit=N.\n") != std::string::npos);
  // An unknown for another reason gets no such note.
  CannedSolver hard([](const Query &) { return CannedSolver::unknown(false); });
  REQUIRE(Run(ABS, hard).report().find("resource limit") == std::string::npos);
}

TEST_CASE("a proof is vacuous when nothing reaches the obligation", "[verify][Driver]") {
  const char *src = R"(
f(x)
  ensures result == 42
{
  assume x > x;
  return 0;
}
)";
  // Z3 proves the postcondition, and also proves `false` there, using the assume.
  CannedSolver solver([](const Query &q) { return isReach(q) ? CannedSolver::unsat({"asm!0"}) : CannedSolver::unsat(); });
  Run run(src, solver);
  auto &r = run.res.functions[0].obligations[0];
  REQUIRE(r.proved());
  REQUIRE(r.vacuous);
  REQUIRE(r.uses == std::vector<std::string>{"asm!0"});
  REQUIRE(solver.asked.size() == 2); // the reason is known: no third question, even with --ptrust
  std::string report = run.report();
  REQUIRE(report.find("  proved  line 6: postcondition   VACUOUS\n") != std::string::npos);
  REQUIRE(report.find("Assumed without proof:\n  assume (line 5) in f\n") != std::string::npos);
}

TEST_CASE("a missing clause fails without asking, and is never assumed", "[verify][Driver]") {
  const char *src = R"(
f(n)
  requires n >= 0
  ensures result == n
{
  var i;
  i = 0;
  while (i < n)
    invariant i <= n
  {
    i = i + 1;
  }
  return i;
}
)";
  CannedSolver solver(agreeable);
  Run run(src, solver);
  auto &obs = run.res.functions[0].obligations;
  REQUIRE(obs.size() == 4);
  REQUIRE(obs[1].ob.kind == ObKind::MissingLoopMeasure);
  REQUIRE(obs[1].answer.verdict == Verdict::Sat);
  REQUIRE(obs[1].asked.empty());
  // Three real obligations: a verdict and a reachability question each.
  REQUIRE(solver.asked.size() == 6);
  REQUIRE(run.report().find("  FAILED  line 8: loop needs a decreases clause\n") != std::string::npos);
  // The missing clause is never offered as something a proof may assume,
  // so the obligations after it are proved (or not) on their own.
  for (auto &q : solver.asked)
    for (auto &[flag, name] : q.flags) REQUIRE(name != "ob!1");
  REQUIRE(obs[3].proved());
  REQUIRE_FALSE(run.res.ok());
}

TEST_CASE("unreachable only because an earlier obligation failed is not vacuous", "[verify][Driver]") {
  const char *src = R"(
f(x)
  ensures result == 42
{
  assert x > x;
  return 0;
}
)";
  // The assertion (#0) fails.  The postcondition (#1) assumes it, so it is
  // proved, and nothing reaches it: but only because of #0, which is
  // already reported.
  CannedSolver solver([](const Query &q) {
    if (isReach(q)) return CannedSolver::unsat({"ob!0"});
    return isVerdict(q) && smt(q.goal).find("42") == std::string::npos ? CannedSolver::sat({"1"})
                                                                         : CannedSolver::unsat();
  });
  Run run(src, solver);
  auto &obs = run.res.functions[0].obligations;
  REQUIRE(obs.size() == 2);
  REQUIRE_FALSE(obs[0].proved());
  REQUIRE(obs[1].proved());
  REQUIRE_FALSE(obs[1].vacuous);
  std::string report = run.report();
  REQUIRE(report.find("VACUOUS") == std::string::npos);
  REQUIRE(report.find("  FAILED  line 5: assertion   when x = 1\n") != std::string::npos);
}

TEST_CASE("a spec function that does not terminate has its definition withheld", "[verify][Driver]") {
  const char *src = R"(
spec bad(n)
  decreases n
{
  return bad(n) + 1;
}
spec good(n)
{
  return n + 1;
}
f(x)
  ensures result == bad(x) + good(x)
{
  return x;
}
)";
  // The first question is bad's termination check; it fails.
  int n = 0;
  CannedSolver solver([&](const Query &q) { return n++ == 0 ? CannedSolver::sat() : agreeable(q); });
  Run run(src, solver);
  REQUIRE(run.res.specChecks.size() == 1);
  REQUIRE(run.res.specChecks[0].first == "bad");
  REQUIRE(run.res.specChecks[0].second.ob.kind == ObKind::CallMeasure);
  REQUIRE_FALSE(run.res.specs[0].terminates);
  REQUIRE_FALSE(run.res.specs[0].used);
  REQUIRE(run.res.specs[1].used);
  // f's query mentions both, and gets good's definition only.
  auto &bg = run.res.background;
  REQUIRE(bg.axioms.count("good"));
  REQUIRE_FALSE(bg.axioms.count("bad"));
  REQUIRE(definitionsNeeded(bg, run.res.functions[0].obligations[0].query) == std::set<std::string>{"good"});
  std::string report = run.report();
  REQUIRE(report.find("spec bad: does not terminate; its definition is not used\n") != std::string::npos);
  REQUIRE(report.find("  definition of good: not recursive\n") != std::string::npos);

  // --trust-specs uses it anyway, and says so.
  n = 0;
  VCGen::Options trusting;
  trusting.trustSpecs = true;
  Run trusted(src, solver, trusting);
  REQUIRE(trusted.res.background.axioms.count("bad"));
  REQUIRE(trusted.report().find("definition of bad: does not terminate (--trust-specs)") != std::string::npos);
}

TEST_CASE("the termination check of a spec function uses no spec definitions", "[verify][Driver]") {
  const char *src = R"(
spec down(n)
  decreases n
{
  var r;
  r = 0;
  if (n > 0) { r = down(n - 1); }
  return r;
}
f(x)
  ensures result == x
{
  return x;
}
)";
  struct Recording : Solver {
    std::vector<size_t> definitionsAvailable;
    Answer check(const Background &bg, const Query &q) override {
      definitionsAvailable.push_back(bg.axioms.size());
      return agreeable(q);
    }
    std::string version() override { return "recording"; }
  } solver;
  std::stringstream stream(src);
  auto ast = ASTHelper::build_ast(stream);
  auto sem = SemanticAnalysis::analyze(ast.get());
  VerifyResult res = verify(ast.get(), sem->getTypeResults(), solver, {});
  REQUIRE(res.specChecks.size() == 1);
  REQUIRE(solver.definitionsAvailable.front() == 0); // down's own definition is not available to its check
  REQUIRE(solver.definitionsAvailable.back() == 1);
}

TEST_CASE("--vfn checks one function; its callees are then not checked in this run", "[verify][Driver]") {
  const char *src = R"(
g(x)
  ensures result == x
{
  return x;
}
f(x)
  ensures result == x
{
  var r;
  r = g(x);
  return r;
}
)";
  CannedSolver solver([](const Query &q) {
    if (isReach(q)) return CannedSolver::sat();
    return isUses(q) ? CannedSolver::unsat({"asm!0"}) : CannedSolver::unsat();
  });
  VCGen::Options opt;
  opt.fn = "f";
  opt.ptrust = true;
  Run run(src, solver, opt);
  REQUIRE(run.res.functions.size() == 1);
  REQUIRE(run.res.functions[0].name == "f");
  REQUIRE(run.res.calls.count({"f", "g"}));
  REQUIRE(run.report().find("  => verified, assuming: contract of g (not checked in this run)\n") != std::string::npos);
}

TEST_CASE("the report lists obligations by line under each function", "[verify][Driver]") {
  const char *src = R"(
half(x)
  requires x >= 0
  ensures result >= 0
{
  return x / 2;
}
main()
{
  var a;
  a = half(4);
  assert a >= 0;
  return 0;
}
)";
  CannedSolver solver(agreeable);
  Run run(src, solver);
  std::string report = run.report();
  size_t at = report.find("half:\n");
  REQUIRE(at != std::string::npos);
  REQUIRE(report.substr(at) .rfind("half:\n"
                                   "  proved  line 6: divisor is not zero\n"
                                   "  proved  line 6: postcondition\n"
                                   "main:\n"
                                   "  proved  line 11: precondition of half(4)\n"
                                   "  proved  line 12: assertion\n"
                                   "  proved  line 13: postcondition\n", 0) == 0);
  REQUIRE(report.find("VC (passive): 5 queries, 0 bytes in p.top.vc.smt2\n") != std::string::npos);
}

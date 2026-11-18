#include "Trust.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace verutop;

namespace {
ObligationResult ob(int index, int line, bool proved, std::vector<std::string> uses = {}, bool usesKnown = true) {
  ObligationResult r;
  r.index = index;
  r.ob.kind = ObKind::Postcondition;
  r.ob.line = line;
  r.answer.verdict = proved ? Verdict::Unsat : Verdict::Sat;
  r.usesKnown = usesKnown;
  r.uses = std::move(uses);
  return r;
}
// A function that ran, with these results; its Proc records the same obligations.
FunctionResult fn(const std::string &name, std::vector<ObligationResult> obs, std::vector<Assumption> assumptions = {}) {
  FunctionResult f;
  f.name = name;
  f.ran = true;
  for (auto &o : obs) f.proc.obs.push_back(o.ob);
  f.proc.assumptions = std::move(assumptions);
  f.obligations = std::move(obs);
  return f;
}
Assumption contractOf(const std::string &g) { return {Assumption::Contract, 5, "contract of " + g + " (line 5)", g}; }
Assumption assumeAt(int line) { return {Assumption::Assume, line, "assume (line " + std::to_string(line) + ")", ""}; }
Assumption pre() { return {Assumption::Pre, 1, "the precondition", ""}; }

std::string verdictOf(const TrustReport &t, const std::string &name) {
  for (auto &f : t.functions)
    if (f.name == name) return f.verdict;
  return "?";
}
std::vector<std::string> linesOf(const TrustReport &t, const std::string &name) {
  for (auto &f : t.functions)
    if (f.name == name) return f.lines;
  return {};
}
bool has(const std::vector<std::string> &xs, const std::string &x) {
  return std::find(xs.begin(), xs.end(), x) != xs.end();
}
} // namespace

TEST_CASE("a function whose proofs use only checked things is verified", "[verify][Trust]") {
  VerifyResult res;
  res.functions = {fn("g", {ob(0, 3, true, {})}), fn("f", {ob(0, 9, true, {"asm!0", "asm!1"})}, {pre(), contractOf("g")})};
  TrustReport t = analyzeTrust(res, false);
  REQUIRE(verdictOf(t, "g") == "verified");
  REQUIRE(verdictOf(t, "f") == "verified");
  // The precondition is not news; the contract is what the proof used.
  REQUIRE(linesOf(t, "f") == std::vector<std::string>{"  line 9: postcondition", "      uses contract of g (line 5)"});
  REQUIRE(linesOf(t, "g").empty());
}

TEST_CASE("a proof that uses an unverified callee's contract rests on it", "[verify][Trust]") {
  VerifyResult res;
  res.functions = {fn("g", {ob(0, 3, false)}), fn("f", {ob(0, 9, true, {"asm!0"})}, {contractOf("g")})};
  res.calls = {{"f", "g"}};
  res.hasEnsures = {{"f", true}, {"g", true}};
  TrustReport t = analyzeTrust(res, false);
  REQUIRE(verdictOf(t, "g") == "not verified: 1 of 1 obligation not proved");
  REQUIRE(verdictOf(t, "f") == "verified, assuming: contract of g (not verified)");
  REQUIRE(has(t.assumed, "contract of g (not verified), used by f"));
}

TEST_CASE("a callee that was skipped or not checked is named as such", "[verify][Trust]") {
  VerifyResult res;
  FunctionResult skipped;
  skipped.name = "g";
  skipped.skipped = "line 4: a call through a function value";
  res.functions = {skipped, fn("f", {ob(0, 9, true, {"asm!0", "asm!1"})}, {contractOf("g"), contractOf("h")})};
  res.calls = {{"f", "g"}, {"f", "h"}};
  res.hasEnsures = {{"g", true}, {"h", true}};
  TrustReport t = analyzeTrust(res, false);
  // h is not in the results at all (--vfn=f).
  REQUIRE(verdictOf(t, "f") == "verified, assuming: contract of g (not checked: line 4: a call through a function "
                               "value); contract of h (not checked in this run)");
}

TEST_CASE("what a verified callee rests on is inherited, and says through whom", "[verify][Trust]") {
  // f calls g calls h; h's proof uses an assume statement.
  VerifyResult res;
  res.functions = {fn("h", {ob(0, 3, true, {"asm!0"})}, {assumeAt(2)}),
                   fn("g", {ob(0, 6, true, {"asm!0"})}, {contractOf("h")}),
                   fn("f", {ob(0, 9, true, {"asm!0"})}, {contractOf("g")})};
  TrustReport t = analyzeTrust(res, false);
  REQUIRE(verdictOf(t, "h") == "verified, assuming: assume (line 2)");
  REQUIRE(verdictOf(t, "g") == "verified, assuming: assume (line 2) (through h)");
  // The first verified callee on the way is the one named.
  REQUIRE(verdictOf(t, "f") == "verified, assuming: assume (line 2) (through h)");
  REQUIRE(has(t.assumed, "assume (line 2) in h"));
}

TEST_CASE("the fixpoint ends on mutual recursion", "[verify][Trust]") {
  VerifyResult res;
  res.functions = {fn("even", {ob(0, 3, true, {"asm!0", "asm!1"})}, {contractOf("odd"), assumeAt(2)}),
                   fn("odd", {ob(0, 8, true, {"asm!0"})}, {contractOf("even")})};
  TrustReport t = analyzeTrust(res, false);
  REQUIRE(verdictOf(t, "even") == "verified, assuming: assume (line 2)");
  REQUIRE(verdictOf(t, "odd") == "verified, assuming: assume (line 2) (through even)");
}

TEST_CASE("an assumption no proof used does not weaken the verdict", "[verify][Trust]") {
  VerifyResult res;
  res.functions = {fn("f", {ob(0, 9, true, {})}, {assumeAt(4)})};
  TrustReport t = analyzeTrust(res, false);
  REQUIRE(verdictOf(t, "f") == "verified");
  // It is still listed in the summary: it was available to every proof.
  REQUIRE(has(t.assumed, "assume (line 4) in f"));
}

TEST_CASE("a vacuous proof is explained and counted", "[verify][Trust]") {
  VerifyResult res;
  ObligationResult r = ob(0, 9, true, {"asm!0", "asm!1"});
  r.vacuous = true;
  res.functions = {fn("f", {r}, {pre(), assumeAt(6)})};
  TrustReport t = analyzeTrust(res, false);
  REQUIRE(t.anyVacuous);
  // When nothing reaches a point, the precondition is part of the reason.
  REQUIRE(linesOf(t, "f") == std::vector<std::string>{
                                 "  line 9: postcondition",
                                 "      VACUOUS: nothing reaches line 9, given the precondition, assume (line 6)"});
  REQUIRE(verdictOf(t, "f") == "verified, assuming: assume (line 6); 1 proof is vacuous");
}

TEST_CASE("when the solver could not say, the verdict says so", "[verify][Trust]") {
  VerifyResult res;
  res.functions = {fn("f", {ob(0, 9, true, {}, false)})};
  TrustReport t = analyzeTrust(res, false);
  REQUIRE(linesOf(t, "f")[1] == "      Z3 did not say what this proof depends on");
  REQUIRE(verdictOf(t, "f") == "verified, assuming: whatever the proof at line 9 depends on");
}

TEST_CASE("a function's own proved obligations are not listed, a failed one is", "[verify][Trust]") {
  VerifyResult res;
  ObligationResult first = ob(0, 4, false), second = ob(1, 9, true, {"ob!0"});
  first.ob.kind = ObKind::Assertion;
  res.functions = {fn("f", {first, second})};
  TrustReport t = analyzeTrust(res, false);
  REQUIRE(has(linesOf(t, "f"), "      uses the obligation at line 4 (assertion)"));
  REQUIRE(verdictOf(t, "f") == "not verified: 1 of 2 obligations not proved");

  res.functions[0].obligations[0].answer.verdict = Verdict::Unsat;
  res.functions[0].obligations[0].uses = {};
  REQUIRE(linesOf(analyzeTrust(res, false), "f").empty());
}

TEST_CASE("the summary: spec definitions, rank, overflow, divergence", "[verify][Trust]") {
  VerifyResult res;
  SpecResult good{"fact", TRUE, TRUE, {}}, plain{"sq", TRUE, TRUE, {}}, bad{"bad", TRUE, TRUE, {}};
  good.recursive = true;
  bad.recursive = true;
  bad.terminates = false;
  bad.used = false;
  res.specs = {bad, good, plain};
  res.rankProved = {{"rank#Tree", true}, {"rank#Odd", false}};
  FunctionResult f = fn("main", {ob(0, 9, true)});
  f.proc.arithmetic = 1;
  f.proc.diverges = {"main", "the loop at line 5"};
  res.functions = {f};

  TrustReport t = analyzeTrust(res, false);
  REQUIRE(t.checked == std::vector<std::string>{"definition of fact: terminates", "definition of sq: not recursive",
                                                 "rank(Tree) >= 1: induction step"});
  REQUIRE(t.assumed == std::vector<std::string>{"1 arithmetic operation in code: no overflow (--overflow checks it)",
                                                 "rank(Odd) >= 1"});
  REQUIRE(t.diverges == std::vector<std::string>{"main", "the loop at line 5 in main"});
  // With --trust-specs the false definition is used, and listed as assumed.
  REQUIRE(has(analyzeTrust(res, true).assumed, "definition of bad: does not terminate (--trust-specs)"));

  res.functions[0].proc.arithmetic = 7;
  REQUIRE(has(analyzeTrust(res, false).assumed, "7 arithmetic operations in code: no overflow (--overflow checks them)"));
}

TEST_CASE("a proof through a false definition rests on it", "[verify][Trust]") {
  VerifyResult res;
  SpecResult bad{"bad", TRUE, TRUE, {}};
  bad.terminates = false;
  res.specs = {bad};
  res.functions = {fn("liar", {ob(0, 9, true, {"def!bad"})})};
  TrustReport t = analyzeTrust(res, true);
  REQUIRE(verdictOf(t, "liar") == "verified, assuming: definition of bad (--trust-specs)");
}

TEST_CASE("unreachable because of a failed obligation is not vacuous", "[verify][Trust]") {
  // Obligation 1 failed; everything after assumes it, so nothing after is reachable.
  REQUIRE_FALSE(vacuous({"asm!0", "ob!1"}, {1}));
  // Unreachable for another reason: that is vacuity.
  REQUIRE(vacuous({"asm!0", "ob!2"}, {1}));
  REQUIRE(vacuous({}, {}));
  REQUIRE(vacuous({"def!bad"}, {0, 1}));
}

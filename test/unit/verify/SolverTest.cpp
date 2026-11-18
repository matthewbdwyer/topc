#include "GclHelpers.h"
#include "Solver.h"

#include <catch2/catch_test_macros.hpp>

using namespace vtest;

namespace {
Query wanting(std::vector<T> values, bool core = false) {
  Query q;
  q.goal = TRUE;
  q.values = std::move(values);
  q.core = core;
  return q;
}
} // namespace

TEST_CASE("the text sent to Z3 asks why, right after check-sat", "[verify][Solver]") {
  Background bg;
  bg.rlimit = 100;
  Query q = wanting({v("x")});
  q.consts = {{"x", "Int"}};
  std::string shown = render(bg, q), sent = z3Input(bg, q);
  REQUIRE(sent.find("(check-sat)\n(get-info :reason-unknown)\n(get-value (|x| ))\n") != std::string::npos);
  // That line is the only difference from what --psmt shows.
  REQUIRE(sent.size() == shown.size() + std::string("(get-info :reason-unknown)\n").size());
  REQUIRE(shown.find("reason-unknown") == std::string::npos);
}

TEST_CASE("unsat is a proof", "[verify][Solver]") {
  Answer a = parseAnswer("unsat\n(:reason-unknown \"\")\n", wanting({}));
  REQUIRE(a.verdict == Verdict::Unsat);
  REQUIRE(a.raw == "unsat");
  REQUIRE_FALSE(a.limited);
  REQUIRE(a.rest.empty());
}

TEST_CASE("sat comes with the values asked for, as TOP would write them", "[verify][Solver]") {
  std::string out = "sat\n(:reason-unknown \"\")\n((|x@0| (- 3))\n (|t@0| (|Node| 1 |Leaf| |Leaf|))\n ((> |x@0| 0) false))\n";
  Answer a = parseAnswer(out, wanting({v("x@0"), v("t@0"), bin(">", v("x@0"), num(0))}));
  REQUIRE(a.verdict == Verdict::Sat);
  REQUIRE(a.values == std::vector<std::string>{"-3", "Node(1, Leaf, Leaf)", "false"});
}

TEST_CASE("unknown says whether the limit was the reason", "[verify][Solver]") {
  Answer limit = parseAnswer("unknown\n(:reason-unknown \"canceled\")\n", wanting({}));
  REQUIRE(limit.verdict == Verdict::Unknown);
  REQUIRE(limit.limited);
  REQUIRE(parseAnswer("unknown\n(:reason-unknown \"max. resource limit exceeded\")\n", wanting({})).limited);
  Answer hard = parseAnswer("unknown\n(:reason-unknown \"smt tactic failed to show goal to be sat/unsat (incomplete quantifiers)\")\n",
                            wanting({}));
  REQUIRE(hard.verdict == Verdict::Unknown);
  REQUIRE_FALSE(hard.limited);
}

TEST_CASE("an unknown answer may still carry candidate values", "[verify][Solver]") {
  Answer a = parseAnswer("unknown\n(:reason-unknown \"(incomplete quantifiers)\")\n((|n| 4))\n", wanting({v("n")}));
  REQUIRE(a.values == std::vector<std::string>{"4"});
}

TEST_CASE("an error after the answer leaves no values, and is kept as text", "[verify][Solver]") {
  Answer a = parseAnswer("unknown\n(:reason-unknown \"canceled\")\n(error \"line 9 column 20: model is not available\")\n",
                         wanting({v("n")}));
  REQUIRE(a.values.empty());
  REQUIRE(a.rest.find("model is not available") != std::string::npos);
}

TEST_CASE("an unsat core is a list of names", "[verify][Solver]") {
  Answer a = parseAnswer("unsat\n(:reason-unknown \"\")\n(asm!0 ob!2 def!factorial\n rank!Tree)\n", wanting({}, true));
  REQUIRE(a.core == std::vector<std::string>{"asm!0", "ob!2", "def!factorial", "rank!Tree"});
  // Not asked for: not read.
  REQUIRE(parseAnswer("unsat\n(:reason-unknown \"\")\n(asm!0)\n", wanting({})).core.empty());
}

TEST_CASE("anything else is an error, reported as the solver wrote it", "[verify][Solver]") {
  Answer a = parseAnswer("(error \"line 3 column 7: unknown constant |y|\")\nsat\n", wanting({}));
  REQUIRE(a.verdict == Verdict::Error);
  REQUIRE(a.raw.find("unknown constant") != std::string::npos);
  Answer none = parseAnswer("", wanting({}));
  REQUIRE(none.verdict == Verdict::Error);
  REQUIRE(none.raw == "no answer");
}

TEST_CASE("the canned solver answers by rule and records what it was asked", "[verify][Solver]") {
  CannedSolver solver([](const Query &q) { return q.core ? CannedSolver::unsat({"asm!0"}) : CannedSolver::sat({"7"}); });
  Background bg;
  REQUIRE(solver.check(bg, wanting({v("x")})).values == std::vector<std::string>{"7"});
  REQUIRE(solver.check(bg, wanting({}, true)).core == std::vector<std::string>{"asm!0"});
  REQUIRE(solver.asked.size() == 2);
  REQUIRE(solver.asked[1].core);
  REQUIRE(CannedSolver::unknown(true).limited);
}

TEST_CASE("a batch is answered in the order asked, whatever the number of jobs", "[verify][Solver]") {
  // Query i has goal i; the rule answers sat with that number.
  std::vector<Query> queries(40);
  for (size_t i = 0; i < queries.size(); ++i) queries[i].goal = num((long long)i);
  std::vector<const Query *> qs;
  for (auto &q : queries) qs.push_back(&q);
  Background bg;
  for (int jobs : {1, 2, 4, 9, 0}) {
    INFO("jobs " << jobs);
    CannedSolver solver([](const Query &q) { return CannedSolver::sat({std::to_string(q.goal->num)}); });
    auto answers = checkAll(solver, bg, qs, jobs);
    REQUIRE(answers.size() == queries.size());
    for (size_t i = 0; i < answers.size(); ++i) REQUIRE(answers[i].values == std::vector<std::string>{std::to_string(i)});
    REQUIRE(solver.asked.size() == queries.size()); // each exactly once
  }
  CannedSolver none([](const Query &) { return CannedSolver::unsat(); });
  REQUIRE(checkAll(none, bg, {}, 4).empty());
  REQUIRE(defaultJobs() >= 1);
  REQUIRE(defaultJobs() <= 4);
}

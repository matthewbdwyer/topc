#include "GclHelpers.h"
#include "Lowering.h"
#include "ProgramHelper.h"
#include "Support.h"

#include <catch2/catch_test_macros.hpp>

using namespace verutop;
using vtest::Program;

namespace {
// The guarded commands of one function, as --pgcl prints them, with the
// comment column removed.
std::string lowered(const Program &p, const std::string &fn, bool overflow = false, Proc *out = nullptr) {
  Lower low(p.ast.get(), p.enc, overflow);
  Cycles cycles(low.calls());
  low.recursion(cycles);
  Proc proc = low.proc(p.fn(fn));
  if (out) *out = proc;
  std::istringstream in(vtest::text(proc.body));
  std::string line, text;
  while (std::getline(in, line)) {
    auto at = line.find("//");
    if (at != std::string::npos) line = line.substr(0, at);
    while (!line.empty() && line.back() == ' ') line.pop_back();
    text += line + "\n";
  }
  return text;
}
std::vector<std::string> kinds(const Proc &p) {
  std::vector<std::string> ks;
  for (auto &o : p.obs) ks.push_back(o.what());
  return ks;
}
} // namespace

TEST_CASE("a function: assume requires, the body, result, assert ensures", "[verify][Lowering]") {
  Program p(R"(
inc(x)
  requires x >= 0
  ensures result > x
{
  var y;
  y = x + 1;
  return y;
}
)");
  Proc proc;
  REQUIRE(lowered(p, "inc", false, &proc) == "assume x >= 0;\n"
                                             "y := 0;\n"
                                             "y := x + 1;\n"
                                             "result := y;\n"
                                             "assert result > x;\n");
  REQUIRE(kinds(proc) == std::vector<std::string>{"postcondition"});
  REQUIRE(proc.formals == std::vector<std::string>{"x"});
  // The precondition is tracked, so a proof can say whether it used it.
  REQUIRE(proc.assumptions.size() == 1);
  REQUIRE(proc.assumptions[0].kind == Assumption::Pre);
  // Without --overflow the addition is assumed not to overflow, and counted.
  REQUIRE(proc.arithmetic == 1);
}

TEST_CASE("an if is a choice between two guarded branches", "[verify][Lowering]") {
  Program p(R"(
abs(x)
  ensures result >= 0
{
  var r;
  if (x < 0) { r = 0 - x; } else { r = x; }
  return r;
}
)");
  REQUIRE(lowered(p, "abs") == "r := 0;\n"
                               "{\n"
                               "  assume x < 0;\n"
                               "  r := 0 - x;\n"
                               "} [] {\n"
                               "  assume !(x < 0);\n"
                               "  r := x;\n"
                               "}\n"
                               "result := r;\n"
                               "assert result >= 0;\n");
}

TEST_CASE("a loop is cut: invariant in, an arbitrary iteration, invariant out", "[verify][Lowering]") {
  Program p(R"(
count(n)
  requires n >= 0
  ensures result == n
{
  var i;
  i = 0;
  while (i < n)
    invariant i <= n
    decreases n - i
  {
    i = i + 1;
  }
  return i;
}
)");
  Proc proc;
  REQUIRE(lowered(p, "count", false, &proc) == "assume n >= 0;\n"
                                               "i := 0;\n"
                                               "i := 0;\n"
                                               "assert i <= n;\n"
                                               "havoc i;\n"
                                               "assume i <= n;\n"
                                               "{\n"
                                               "  assume i < n;\n"
                                               "  measure#0 := n - i;\n"
                                               "  i := i + 1;\n"
                                               "  assert i <= n;\n"
                                               "  assert 0 <= measure#0 && n - i < measure#0;\n"
                                               "  assume false;\n"
                                               "} [] {\n"
                                               "  assume !(i < n);\n"
                                               "}\n"
                                               "result := i;\n"
                                               "assert result == n;\n");
  REQUIRE(kinds(proc) == std::vector<std::string>{"loop invariant holds on entry", "loop invariant is preserved",
                                                  "loop measure decreases", "postcondition"});
}

TEST_CASE("a loop without decreases gets a missing-clause obligation", "[verify][Lowering]") {
  Program p(R"(
f(n)
{
  var i;
  i = 0;
  while (i < n)
    invariant i >= 0
  {
    i = i + 1;
  }
  return i;
}
)");
  Proc proc;
  lowered(p, "f", false, &proc);
  REQUIRE(proc.obs[1].kind == ObKind::MissingLoopMeasure);
  REQUIRE(proc.obs[1].missing());
}

TEST_CASE("a call is its contract: assert requires, havoc the result, assume ensures", "[verify][Lowering]") {
  Program p(R"(
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
  return a;
}
)");
  Proc proc;
  REQUIRE(lowered(p, "main", false, &proc) == "a := 0;\n"
                                              "assert 4 >= 0;\n"
                                              "havoc a;\n"
                                              "assume a >= 0;\n"
                                              "result := a;\n"
                                              "assert true;\n");
  REQUIRE(proc.obs[0].kind == ObKind::Precondition);
  REQUIRE(proc.obs[0].what() == "precondition of half(4)");
  REQUIRE(proc.obs[0].callee == "half");
  // The callee's postcondition is an assumption a proof can be asked about.
  REQUIRE(proc.assumptions.size() == 1);
  REQUIRE(proc.assumptions[0].kind == Assumption::Contract);
  REQUIRE(proc.assumptions[0].callee == "half");
  // A division is an obligation in the callee.
  Proc callee;
  lowered(p, "half", false, &callee);
  REQUIRE(kinds(callee) == std::vector<std::string>{"divisor is not zero", "postcondition"});
}

TEST_CASE("recursion needs a measure, around a cycle too", "[verify][Lowering]") {
  Program p(R"(
down(n)
  requires n >= 0
  decreases n
{
  var r;
  r = 0;
  if (n > 0) { r = down(n - 1); }
  return r;
}
even(n)
  requires n >= 0
  decreases n
{
  var r;
  r = 1;
  if (n > 0) { r = odd(n - 1); }
  return r;
}
odd(n)
  requires n >= 0
{
  var r;
  r = 0;
  if (n > 0) { r = even(n - 1); }
  return r;
}
)");
  Proc down, even, odd;
  lowered(p, "down", false, &down);
  REQUIRE(down.obs[0].kind == ObKind::CallMeasure);
  REQUIRE(lowered(p, "down").find("assert 0 <= n - 1 && n - 1 < n;") != std::string::npos);
  // odd has no decreases, so the cycle's calls are missing clauses that name it.
  lowered(p, "even", false, &even);
  lowered(p, "odd", false, &odd);
  REQUIRE(even.obs[0].kind == ObKind::MissingMutualCallMeasure);
  REQUIRE(even.obs[0].what() == "mutually recursive call needs a decreases clause on odd");
  REQUIRE(odd.obs[0].what() == "mutually recursive call needs a decreases clause on odd");
}

TEST_CASE("--overflow turns each arithmetic operation in code into an obligation", "[verify][Lowering]") {
  Program p(R"(
inc(x)
  requires x >= 0
  ensures result == x + 1
{
  return x + 1;
}
)");
  Proc proc;
  std::string text = lowered(p, "inc", true, &proc);
  REQUIRE(text.find("assume INT_MIN <= x && x <= INT_MAX;") != std::string::npos);
  REQUIRE(text.find("assert INT_MIN <= x + 1 && x + 1 <= INT_MAX;") != std::string::npos);
  // The x + 1 in ensures is a specification: mathematical, no obligation.
  REQUIRE(kinds(proc) == std::vector<std::string>{"x + 1 does not overflow", "postcondition"});
  REQUIRE(proc.arithmetic == 0);
}

TEST_CASE("a written borrow is an in-out parameter of the call", "[verify][Lowering]") {
  Program p(R"(
set(p, v)
  ensures *p == v
{
  *p = v;
  return 0;
}
main()
{
  var a, d;
  a = 1;
  d = set(&a, 5);
  assert a == 5;
  return a;
}
)");
  // *p = v assigns p; the caller's a takes a new value a' that the
  // postcondition describes.
  REQUIRE(lowered(p, "set").find("p := v;\n") != std::string::npos);
  std::string text = lowered(p, "main");
  REQUIRE(text.find("havoc d, a';\n"
                    "assume a' == 5;\n"
                    "a := a';\n"
                    "assert a == 5;\n") != std::string::npos);
}

TEST_CASE("error is an obligation that nothing reaches it", "[verify][Lowering]") {
  Program p(R"(
f(x)
  requires x > 0
{
  if (x <= 0) { error 1; }
  return x;
}
)");
  Proc proc;
  std::string text = lowered(p, "f", false, &proc);
  REQUIRE(text.find("  assert false;\n  assume false;\n") != std::string::npos);
  REQUIRE(proc.obs[0].kind == ObKind::ErrorUnreachable);
  REQUIRE(proc.obs[0].unreachable());
}

TEST_CASE("the calls of a program are found by lowering it", "[verify][Lowering]") {
  Program p(R"(
g(x) { return x; }
f(x) {
  var r;
  r = g(x);
  r = f(r);
  return r;
}
)");
  Lower low(p.ast.get(), p.enc, false);
  REQUIRE(low.calls() == CallEdges{{"f", "g"}, {"f", "f"}});
}

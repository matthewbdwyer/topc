#include "GclHelpers.h"
#include "Passivize.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

using namespace vtest;

namespace {
bool has(const std::vector<std::string> &xs, const std::string &x) {
  return std::find(xs.begin(), xs.end(), x) != xs.end();
}
} // namespace

TEST_CASE("an assignment becomes an equation on a new version", "[verify][Passivize]") {
  // x := x + 1; x := x * 2; assert x > 0
  Proc p = procOf({"x"}, gSeq({gAssign("x", bin("+", v("x"), num(1)), 1), gAssign("x", bin("*", v("x"), num(2)), 2),
                               check(bin(">", v("x"), num(0)), 0, 3)}),
                  1);
  Passive pv = passivize(p);
  REQUIRE(pv.entry == std::vector<std::string>{"x@0"});
  REQUIRE(pv.versions == std::vector<std::string>{"x@0", "x@1", "x@2"});
  auto &ks = pv.prog.as<Seq>()->kids;
  REQUIRE(ks.size() == 3);
  REQUIRE(flat(ks[0].as<Assume>()->p) == "x@1 == x@0 + 1");
  REQUIRE(ks[0].as<Assume>()->defines == "x@1");
  REQUIRE(flat(ks[0].as<Assume>()->src) == "x + 1"); // the source expression, for traces
  REQUIRE(flat(ks[1].as<Assume>()->p) == "x@2 == x@1 * 2");
  REQUIRE(flat(ks[2].as<Assert>()->p) == "x@2 > 0");
  REQUIRE(ks[2].as<Assert>()->id == 0);
}

TEST_CASE("after a choice both branches define one merged version", "[verify][Passivize]") {
  // if (x > 0) y := 1 else y := 2; assert y > 0
  Proc p = procOf({"x"}, gSeq({ifElse(bin(">", v("x"), num(0)), gAssign("y", num(1), 2), gAssign("y", num(2), 3)),
                               check(bin(">", v("y"), num(0)), 0, 4)}),
                  1);
  Passive pv = passivize(p);
  auto &ks = pv.prog.as<Seq>()->kids;
  auto &arms = ks[0].as<Choice>()->kids;
  auto last = [](const G &arm) { return flat(arm.as<Seq>()->kids.back().as<Assume>()->p); };
  // y@0 in the then branch, y@1 in the else branch, y@2 after the join.
  REQUIRE(last(arms[0]) == "y@2 == y@0");
  REQUIRE(last(arms[1]) == "y@2 == y@1");
  REQUIRE(arms[0].as<Seq>()->kids.back().as<Assume>()->note == "merge y");
  REQUIRE(flat(ks[1].as<Assert>()->p) == "y@2 > 0");
  // x is not assigned in either branch, so it needs no merge.
  REQUIRE(pv.versions == std::vector<std::string>{"x@0", "y@0", "y@1", "y@2"});
}

TEST_CASE("a branch that ends in assume false has nothing to merge", "[verify][Passivize]") {
  // { assume c; y := 1; assume false } [] { assume !c; y := 2 }; assert y == 2
  T c = bin(">", v("x"), num(0));
  G dead = gSeq({guard(c), gAssign("y", num(1), 2), gAssume(FALSE, 2, "this path ends")});
  G live = gSeq({guard(Not(c)), gAssign("y", num(2), 3)});
  Proc p = procOf({"x"}, gSeq({gChoice(dead, live, 1), check(Eq(v("y"), num(2)), 0, 4)}), 1);
  Passive pv = passivize(p);
  auto &ks = pv.prog.as<Seq>()->kids;
  // The code after the choice sees the live branch's version directly.
  REQUIRE(flat(ks[1].as<Assert>()->p) == "y@1 == 2");
  REQUIRE(!has(pv.versions, "y@2"));
}

TEST_CASE("havoc starts a version; old(x) is the entry version", "[verify][Passivize]") {
  Havoc h;
  h.vars = {"x"};
  h.sorts = {"Int"};
  h.note = "an arbitrary iteration";
  Proc p = procOf({"x"}, gSeq({h, check(bin(">=", v("x"), v("old(x)")), 0)}), 1);
  Passive pv = passivize(p);
  auto &ks = pv.prog.as<Seq>()->kids;
  REQUIRE(ks[0].as<Havoc>()->vars == std::vector<std::string>{"x@1"});
  REQUIRE(ks[0].as<Havoc>()->note == "havoc x");
  REQUIRE(flat(ks[1].as<Assert>()->p) == "x@1 >= x@0");
}

TEST_CASE("a variable first read without a definition gets a version", "[verify][Passivize]") {
  // A local read before any assignment: it is unconstrained.
  Proc p = procOf({}, check(bin(">", v("t"), num(0)), 0), 1);
  p.sorts["t"] = "|Tree|";
  Passive pv = passivize(p);
  REQUIRE(pv.versions == std::vector<std::string>{"t@0"});
  REQUIRE(pv.sorts.at("t@0") == "|Tree|");
}

#include "SimpleTermImpl.h"
#include "TermUnifier.h"

#include <catch2/catch_test_macros.hpp>
#include <memory>

// Helpers
static std::shared_ptr<SimpleVar> var(const std::string &name) {
  return std::make_shared<SimpleVar>(name);
}
static std::shared_ptr<SimpleCons> cons(const std::string &f,
                                        std::vector<std::shared_ptr<Term>> s = {}) {
  return std::make_shared<SimpleCons>(f, std::move(s));
}

TEST_CASE("VarUnifiesWithVar", "[TermUnifier][standalone]") {
  auto a = var("a");
  auto b = var("b");
  TermUnifier u;
  u.addConstraint(a, b);
  u.solve();
  // Both must share a representative
  auto ra = u.find(a);
  auto rb = u.find(b);
  REQUIRE(ra->equals(*rb));
}

TEST_CASE("VarUnifiesWithCons", "[TermUnifier][standalone]") {
  auto a = var("a");
  auto f = cons("f");
  TermUnifier u;
  u.addConstraint(a, f);
  u.solve();
  auto rep = u.find(a);
  REQUIRE(rep->getFunctor() == "f");
  REQUIRE_FALSE(rep->isVariable());
}

TEST_CASE("ConsUnifiesWithVar", "[TermUnifier][standalone]") {
  auto a = var("a");
  auto g = cons("g");
  TermUnifier u;
  u.addConstraint(g, a);
  u.solve();
  auto rep = u.find(a);
  REQUIRE(rep->getFunctor() == "g");
  REQUIRE_FALSE(rep->isVariable());
}

TEST_CASE("IdenticalConsUnify", "[TermUnifier][standalone]") {
  auto fg1 = cons("f", {cons("g")});
  auto fg2 = cons("f", {cons("g")});
  TermUnifier u;
  u.addConstraint(fg1, fg2);
  REQUIRE_NOTHROW(u.solve());
}

TEST_CASE("ConsSubtermsPropagated", "[TermUnifier][standalone]") {
  auto a = var("a");
  auto intCons = cons("int");
  auto lhs = cons("f", {a});
  auto rhs = cons("f", {intCons});
  TermUnifier u;
  u.addConstraint(lhs, rhs);
  u.solve();
  auto rep = u.find(a);
  REQUIRE(rep->getFunctor() == "int");
}

TEST_CASE("TransitiveBinding", "[TermUnifier][standalone]") {
  auto a = var("a");
  auto b = var("b");
  auto h = cons("h");
  TermUnifier u;
  u.addConstraint(a, b);
  u.addConstraint(b, h);
  u.solve();
  auto rep = u.find(a);
  REQUIRE(rep->getFunctor() == "h");
}

TEST_CASE("FunctorClashThrows", "[TermUnifier][standalone]") {
  auto f = cons("f");
  auto g = cons("g");
  TermUnifier u;
  u.addConstraint(f, g);
  REQUIRE_THROWS_AS(u.solve(), TermUnificationError);
}

TEST_CASE("ArityClashThrows", "[TermUnifier][standalone]") {
  auto x = var("x");
  auto y = var("y");
  auto lhs = cons("f", {x});
  auto rhs = cons("f", {x, y});
  TermUnifier u;
  u.addConstraint(lhs, rhs);
  REQUIRE_THROWS_AS(u.solve(), TermUnificationError);
}

TEST_CASE("SolveAfterMultipleAdds", "[TermUnifier][standalone]") {
  auto a = var("a");
  auto b = var("b");
  auto intCons = cons("int");
  auto boolCons = cons("bool");
  TermUnifier u;
  u.addConstraint(a, intCons);
  u.addConstraint(b, boolCons);
  u.solve();
  REQUIRE(u.find(a)->getFunctor() == "int");
  REQUIRE(u.find(b)->getFunctor() == "bool");
}

TEST_CASE("EmptyConstraintSet", "[TermUnifier][standalone]") {
  TermUnifier u;
  REQUIRE_NOTHROW(u.solve());
}

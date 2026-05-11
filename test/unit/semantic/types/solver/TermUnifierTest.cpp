#include "MockTerm.h"
#include "TermUnifier.h"
#include <catch2/catch_test_macros.hpp>

inline std::shared_ptr<Term> var(const std::string &name) {
  return std::make_shared<MockVar>(name);
}

inline std::shared_ptr<Term> con(const std::string &name) {
  return std::make_shared<MockCons>(name);
}

inline std::shared_ptr<Term> con1(const std::string &name, std::shared_ptr<Term> arg) {
  return std::make_shared<MockCons>(name, std::vector<std::shared_ptr<Term>>{arg});
}

inline std::shared_ptr<Term> con2(const std::string &name, std::shared_ptr<Term> arg1, std::shared_ptr<Term> arg2) {
  return std::make_shared<MockCons>(name, std::vector<std::shared_ptr<Term>>{arg1, arg2});
}

TEST_CASE("TermUnifier: empty constraints", "[TermUnifier]") {
  TermUnifier u;
  REQUIRE_NOTHROW(u.solve());
}

TEST_CASE("TermUnifier: variable binds to constant", "[TermUnifier]") {
  auto x = var("X");
  auto a = con("a");
  TermUnifier u;
  u.addConstraint(x, a);
  u.solve();
  REQUIRE(u.apply(x)->getFunctor() == "a");
  REQUIRE_FALSE(u.apply(x)->isVariable());
}

TEST_CASE("TermUnifier: constant binds to variable", "[TermUnifier]") {
  auto x = var("X");
  auto a = con("a");
  TermUnifier u;
  u.addConstraint(a, x);
  u.solve();
  REQUIRE(u.apply(x)->getFunctor() == "a");
}

TEST_CASE("TermUnifier: variable binds to variable", "[TermUnifier]") {
  auto x = var("X");
  auto y = var("Y");
  auto a = con("a");
  TermUnifier u;
  u.addConstraint(x, y);
  u.addConstraint(y, a);
  u.solve();
  REQUIRE(u.apply(x)->getFunctor() == "a");
  REQUIRE(u.apply(y)->getFunctor() == "a");
}

TEST_CASE("TermUnifier: same constant unifies", "[TermUnifier]") {
  TermUnifier u;
  u.addConstraint(con("a"), con("a"));
  REQUIRE_NOTHROW(u.solve());
}

TEST_CASE("TermUnifier: different constants fail", "[TermUnifier]") {
  TermUnifier u;
  u.addConstraint(con("a"), con("b"));
  REQUIRE_THROWS_AS(u.solve(), TermUnificationError);
}

TEST_CASE("TermUnifier: compound terms unify recursively", "[TermUnifier]") {
  auto x = var("X");
  auto y = var("Y");
  TermUnifier u;
  u.addConstraint(con2("f", x, y), con2("f", con("a"), con("b")));
  u.solve();
  REQUIRE(u.apply(x)->getFunctor() == "a");
  REQUIRE(u.apply(y)->getFunctor() == "b");
}

TEST_CASE("TermUnifier: nested compound terms", "[TermUnifier]") {
  auto x = var("X");
  TermUnifier u;
  u.addConstraint(con1("f", con1("g", x)), con1("f", con1("g", con("a"))));
  u.solve();
  REQUIRE(u.apply(x)->getFunctor() == "a");
}

TEST_CASE("TermUnifier: different arity fails", "[TermUnifier]") {
  TermUnifier u;
  u.addConstraint(con1("f", con("a")), con2("f", con("a"), con("a")));
  REQUIRE_THROWS_AS(u.solve(), TermUnificationError);
}

TEST_CASE("TermUnifier: different functor fails", "[TermUnifier]") {
  TermUnifier u;
  u.addConstraint(con1("f", con("a")), con1("g", con("a")));
  REQUIRE_THROWS_AS(u.solve(), TermUnificationError);
}

// Note: no occurs check — cyclic constraints are permitted and handled in
// TipTermClosure::close() via TipMu.  The two former occurs-check tests
// (direct and indirect) have been removed in Phase 4.5.

TEST_CASE("TermUnifier: transitivity", "[TermUnifier]") {
  auto x = var("X");
  auto y = var("Y");
  auto z = var("Z");
  TermUnifier u;
  u.addConstraint(x, y);
  u.addConstraint(y, z);
  u.addConstraint(z, con("a"));
  u.solve();
  REQUIRE(u.apply(x)->getFunctor() == "a");
  REQUIRE(u.apply(y)->getFunctor() == "a");
  REQUIRE(u.apply(z)->getFunctor() == "a");
}

TEST_CASE("TermUnifier: find — bound variable resolves to proper type", "[TermUnifier]") {
  auto x = var("X");
  auto a = con("a");
  TermUnifier u;
  u.addConstraint(x, a);
  u.solve();
  // x is bound — find(x) returns the proper-type representative
  REQUIRE_FALSE(u.find(x)->isVariable());
  REQUIRE(u.find(x)->getFunctor() == "a");
}

TEST_CASE("TermUnifier: find — unbound variable returns itself", "[TermUnifier]") {
  auto x = var("X");
  auto y = var("Y");
  TermUnifier u;
  u.addConstraint(x, con("a"));
  u.solve();
  // y was never constrained — find(y) returns a value-equal variable
  REQUIRE(u.find(y)->isVariable());
  REQUIRE(u.find(y)->getFunctor() == "Y");
}

TEST_CASE("TermUnifier: find — resolves transitive chain", "[TermUnifier]") {
  auto x = var("X");
  auto y = var("Y");
  TermUnifier u;
  u.addConstraint(x, y);
  u.addConstraint(y, con("a"));
  u.solve();
  REQUIRE(u.find(x)->getFunctor() == "a");
}

TEST_CASE("TermUnifier: apply preserves unbound variables", "[TermUnifier]") {
  auto x = var("X");
  auto y = var("Y");
  TermUnifier u;
  u.addConstraint(x, con("a"));
  u.solve();
  auto result = u.apply(con2("f", x, y));
  auto subs = result->getSubterms();
  REQUIRE(subs[0]->getFunctor() == "a");
  REQUIRE(subs[1]->isVariable());
  REQUIRE(subs[1]->getFunctor() == "Y");
}

TEST_CASE("TermUnifier: error contains terms", "[TermUnifier]") {
  TermUnifier u;
  u.addConstraint(con("a"), con("b"));
  try {
    u.solve();
    FAIL("Expected TermUnificationError");
  } catch (const TermUnificationError &e) {
    REQUIRE(e.getLhs()->getFunctor() == "a");
    REQUIRE(e.getRhs()->getFunctor() == "b");
  }
}

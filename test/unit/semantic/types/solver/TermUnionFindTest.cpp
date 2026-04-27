#include "MockTerm.h"
#include "TermUnionFind.h"
#include <catch2/catch_test_macros.hpp>
#include <memory>

// Helpers
static std::shared_ptr<Term> v(const std::string &n) {
  return std::make_shared<MockVar>(n);
}
static std::shared_ptr<Term> c(const std::string &n) {
  return std::make_shared<MockCons>(n);
}

TEST_CASE("TermUnionFind: new term is its own representative", "[TermUnionFind]") {
  TermUnionFind uf;
  auto x = v("X");
  auto rep = uf.find(x);
  // Unregistered term added as own root; representative is value-equal to x
  REQUIRE(rep->isVariable());
  REQUIRE(rep->getFunctor() == "X");
}

TEST_CASE("TermUnionFind: add is idempotent", "[TermUnionFind]") {
  TermUnionFind uf;
  auto x = v("X");
  uf.add(x);
  uf.add(x);
  auto rep = uf.find(x);
  REQUIRE(rep->getFunctor() == "X");
}

TEST_CASE("TermUnionFind: quick_union — proper type wins as representative", "[TermUnionFind]") {
  TermUnionFind uf;
  auto x = v("X");
  auto a = c("a");
  // x → a (proper type becomes root)
  uf.quick_union(x, a);
  REQUIRE(uf.find(x)->getFunctor() == "a");
  REQUIRE_FALSE(uf.find(x)->isVariable());
}

TEST_CASE("TermUnionFind: quick_union — two variables", "[TermUnionFind]") {
  TermUnionFind uf;
  auto x = v("X");
  auto y = v("Y");
  uf.quick_union(x, y); // Y becomes root
  // find(x) == find(y) by value
  REQUIRE(uf.find(x)->equals(*uf.find(y)));
}

TEST_CASE("TermUnionFind: transitive chain x→y→a", "[TermUnionFind]") {
  TermUnionFind uf;
  auto x = v("X");
  auto y = v("Y");
  auto a = c("a");
  uf.quick_union(x, y);
  uf.quick_union(y, a);
  // Both x and y should resolve to a
  REQUIRE(uf.find(x)->getFunctor() == "a");
  REQUIRE(uf.find(y)->getFunctor() == "a");
}

TEST_CASE("TermUnionFind: connected — same class", "[TermUnionFind]") {
  TermUnionFind uf;
  auto x = v("X");
  auto a = c("a");
  uf.quick_union(x, a);
  REQUIRE(uf.connected(x, a));
}

TEST_CASE("TermUnionFind: connected — different classes", "[TermUnionFind]") {
  TermUnionFind uf;
  auto x = v("X");
  auto y = v("Y");
  REQUIRE_FALSE(uf.connected(x, y));
}

TEST_CASE("TermUnionFind: value-based equality — fresh pointer for same term", "[TermUnionFind]") {
  TermUnionFind uf;
  auto x1 = v("X");
  auto x2 = v("X"); // different shared_ptr, same value
  auto a = c("a");
  uf.quick_union(x1, a);
  // find via x2 (different pointer, same functor) should still return a
  REQUIRE(uf.find(x2)->getFunctor() == "a");
}

TEST_CASE("TermUnionFind: three-way unification", "[TermUnionFind]") {
  TermUnionFind uf;
  auto x = v("X");
  auto y = v("Y");
  auto z = v("Z");
  auto a = c("a");
  uf.quick_union(x, y);
  uf.quick_union(y, z);
  uf.quick_union(z, a);
  REQUIRE(uf.find(x)->getFunctor() == "a");
  REQUIRE(uf.find(y)->getFunctor() == "a");
  REQUIRE(uf.find(z)->getFunctor() == "a");
  REQUIRE(uf.connected(x, a));
}

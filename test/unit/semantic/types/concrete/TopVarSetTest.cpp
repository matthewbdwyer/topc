#include "TopVar.h"
#include "ASTNumberExpr.h"

#include <catch2/catch_test_macros.hpp>
#include <set>

TEST_CASE("TopVar: two vars wrapping same node are value-equal",
          "[TopVar][TopVarSet]") {
  ASTNumberExpr node(42);
  auto v1 = std::make_shared<TopVar>(&node);
  auto v2 = std::make_shared<TopVar>(&node);

  // operator== uses node pointer — both point to same node, so they are equal.
  REQUIRE(*v1 == *v2);
}

TEST_CASE("TopVar: std::set with TopVarValueCmp deduplicates by value",
          "[TopVar][TopVarSet]") {
  // After Phase 3 TopVarValueCmp fix: two shared_ptrs wrapping the same
  // ASTNode* are treated as the same element because the comparator uses
  // operator< (node pointer ordering) rather than wrapper address.
  ASTNumberExpr node(42);
  auto v1 = std::make_shared<TopVar>(&node);
  auto v2 = std::make_shared<TopVar>(&node);

  TopVarSet s;
  s.insert(v1);
  s.insert(v2);

  REQUIRE(s.size() == 1);
}

TEST_CASE("TopVar: linear-scan value comparison finds both equal vars",
          "[TopVar][TopVarSet]") {
  // With TopVarValueCmp: count() on TopVarSet works correctly by value.
  ASTNumberExpr node(42);
  auto v1 = std::make_shared<TopVar>(&node);
  auto v2 = std::make_shared<TopVar>(&node);

  TopVarSet s;
  s.insert(v1);

  // count() uses TopVarValueCmp, so v2 (same node) is found.
  REQUIRE(s.count(v2) == 1);
}

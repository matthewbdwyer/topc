#include "TipVar.h"
#include "ASTNumberExpr.h"

#include <catch2/catch_test_macros.hpp>
#include <set>

TEST_CASE("TipVar: two vars wrapping same node are value-equal",
          "[TipVar][TipVarSet]") {
  ASTNumberExpr node(42);
  auto v1 = std::make_shared<TipVar>(&node);
  auto v2 = std::make_shared<TipVar>(&node);

  // operator== uses node pointer — both point to same node, so they are equal.
  REQUIRE(*v1 == *v2);
}

TEST_CASE("TipVar: std::set with TipVarValueCmp deduplicates by value",
          "[TipVar][TipVarSet]") {
  // After Phase 3 TipVarValueCmp fix: two shared_ptrs wrapping the same
  // ASTNode* are treated as the same element because the comparator uses
  // operator< (node pointer ordering) rather than wrapper address.
  ASTNumberExpr node(42);
  auto v1 = std::make_shared<TipVar>(&node);
  auto v2 = std::make_shared<TipVar>(&node);

  TipVarSet s;
  s.insert(v1);
  s.insert(v2);

  REQUIRE(s.size() == 1);
}

TEST_CASE("TipVar: linear-scan value comparison finds both equal vars",
          "[TipVar][TipVarSet]") {
  // With TipVarValueCmp: count() on TipVarSet works correctly by value.
  ASTNumberExpr node(42);
  auto v1 = std::make_shared<TipVar>(&node);
  auto v2 = std::make_shared<TipVar>(&node);

  TipVarSet s;
  s.insert(v1);

  // count() uses TipVarValueCmp, so v2 (same node) is found.
  REQUIRE(s.count(v2) == 1);
}

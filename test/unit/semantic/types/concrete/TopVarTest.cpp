#include "TopVar.h"
#include "TopInt.h"
#include "ASTNumberExpr.h"

#include <catch2/catch_test_macros.hpp>

#include <vector>

TEST_CASE("TopVar: check equality with the same underlying node are equal",
          "[TopVar]") {
  ASTNumberExpr n(42);
  TopVar var(&n);
  TopVar var2(&n);
  REQUIRE(var == var2);
}

TEST_CASE("TopVar: check equality with different underlying nodes",
          "[TopVar]") {
  ASTNumberExpr n(99);
  ASTNumberExpr n1(99);
  TopVar var(&n);
  TopVar var2(&n1);
  REQUIRE_FALSE(var == var2);
}

TEST_CASE("TopVar: test TopVar is a TopType",
          "[TopVar]") {
  ASTNumberExpr n(42);
  TopVar var(&n);
  REQUIRE_FALSE(nullptr == dynamic_cast<TopType *>(&var));
}

#include "ASTBinaryExpr.h"
#include "ASTNumberExpr.h"

#include <catch2/catch_test_macros.hpp>

// Shared operands reused across sections.
static std::shared_ptr<ASTNumberExpr> lhs() {
  return std::make_shared<ASTNumberExpr>(1);
}
static std::shared_ptr<ASTNumberExpr> rhs() {
  return std::make_shared<ASTNumberExpr>(2);
}

TEST_CASE("ASTBinaryExpr: getOp returns correct operator strings",
          "[ASTBinaryExpr]") {
  SECTION("addition") {
    ASTBinaryExpr node("+", lhs(), rhs());
    REQUIRE(node.getOp() == "+");
  }
  SECTION("subtraction") {
    ASTBinaryExpr node("-", lhs(), rhs());
    REQUIRE(node.getOp() == "-");
  }
  SECTION("multiplication") {
    ASTBinaryExpr node("*", lhs(), rhs());
    REQUIRE(node.getOp() == "*");
  }
  SECTION("division") {
    ASTBinaryExpr node("/", lhs(), rhs());
    REQUIRE(node.getOp() == "/");
  }
  SECTION("greater-than") {
    ASTBinaryExpr node(">", lhs(), rhs());
    REQUIRE(node.getOp() == ">");
  }
  SECTION("equality") {
    ASTBinaryExpr node("==", lhs(), rhs());
    REQUIRE(node.getOp() == "==");
  }
  SECTION("inequality") {
    ASTBinaryExpr node("!=", lhs(), rhs());
    REQUIRE(node.getOp() == "!=");
  }
}

TEST_CASE("ASTBinaryExpr: operand access returns correct nodes",
          "[ASTBinaryExpr]") {
  auto l = lhs();
  auto r = rhs();
  ASTBinaryExpr node("+", l, r);

  REQUIRE(node.getLeft()  == l.get());
  REQUIRE(node.getRight() == r.get());
}

TEST_CASE("ASTBinaryExpr: getOp string comparison works across instances",
          "[ASTBinaryExpr]") {
  // Two nodes with "+" both return the same string.
  // This must continue to hold after Phase 6 changes return type to const string &.
  ASTBinaryExpr a("+", lhs(), rhs());
  ASTBinaryExpr b("+", lhs(), rhs());
  REQUIRE(a.getOp() == b.getOp());
}

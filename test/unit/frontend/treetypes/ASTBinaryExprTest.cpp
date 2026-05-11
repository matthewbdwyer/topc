#include "ASTBinaryExpr.h"
#include "ASTNumberExpr.h"
#include "InternalError.h"

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

TEST_CASE("ASTBinaryExpr: getOpKind returns correct BinaryOp enum value",
          "[ASTBinaryExpr]") {
  using Op = ASTBinaryExpr::BinaryOp;
  REQUIRE(ASTBinaryExpr("+",  lhs(), rhs()).getOpKind() == Op::Add);
  REQUIRE(ASTBinaryExpr("-",  lhs(), rhs()).getOpKind() == Op::Sub);
  REQUIRE(ASTBinaryExpr("*",  lhs(), rhs()).getOpKind() == Op::Mul);
  REQUIRE(ASTBinaryExpr("/",  lhs(), rhs()).getOpKind() == Op::Div);
  REQUIRE(ASTBinaryExpr(">",  lhs(), rhs()).getOpKind() == Op::Gt);
  REQUIRE(ASTBinaryExpr("==", lhs(), rhs()).getOpKind() == Op::Eq);
  REQUIRE(ASTBinaryExpr("!=", lhs(), rhs()).getOpKind() == Op::Neq);
}

TEST_CASE("ASTBinaryExpr: unknown operator throws InternalError at construction",
          "[ASTBinaryExpr]") {
  REQUIRE_THROWS_AS(ASTBinaryExpr("<", lhs(), rhs()), InternalError);
}

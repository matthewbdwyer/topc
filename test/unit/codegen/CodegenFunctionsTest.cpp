#include "AST.h"
#include "ASTNodeHelpers.h"
#include "CodeGenContext.h"
#include "CodeGenVisitor.h"
#include "InternalError.h"
#include "ParserHelper.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("CodegenFunction: ASTDeclNode throws InternalError on codegen",
          "[CodegenFunctions]") {
  ASTDeclNode node("foo");
  CodeGenContext ctx;
  CodeGenVisitor visitor;
  visitor.setContext(&ctx);
  REQUIRE_THROWS_AS(visitor.dispatch(&node), InternalError);
}

TEST_CASE("CodegenFunction: ASTAssignsStmt throws InternalError on LHS codegen nullptr",
          "[CodegenFunctions]") {
  ASTAssignStmt assignStmt(std::make_shared<nullcodegen::MockASTExpr>(),
                           std::make_shared<ASTInputExpr>());
  CodeGenContext ctx;
  CodeGenVisitor visitor;
  visitor.setContext(&ctx);
  REQUIRE_THROWS_AS(visitor.dispatch(&assignStmt), InternalError);
}

TEST_CASE("CodegenFunction: ASTAssignsStmt throws InternalError on RHS codegen nullptr",
          "[CodegenFunctions]") {
  ASTAssignStmt assignStmt(std::make_shared<ASTInputExpr>(),
                           std::make_shared<nullcodegen::MockASTExpr>());
  CodeGenContext ctx;
  CodeGenVisitor visitor;
  visitor.setContext(&ctx);
  REQUIRE_THROWS_AS(visitor.dispatch(&assignStmt), InternalError);
}

TEST_CASE(
    "CodegenFunction: ASTIfStmt throws InternalError on COND codegen nullptr",
    "[CodegenFunctions]") {
  ASTIfStmt ifStmt(
      std::make_shared<nullcodegen::MockASTExpr>(),
      std::make_shared<ASTReturnStmt>(std::make_shared<ASTNumberExpr>(42)),
      std::make_shared<ASTReturnStmt>(std::make_shared<ASTNumberExpr>(42)));
  CodeGenContext ctx;
  CodeGenVisitor visitor;
  visitor.setContext(&ctx);
  REQUIRE_THROWS_AS(visitor.dispatch(&ifStmt), InternalError);
}

TEST_CASE("CodegenFunction: ASTBinaryExpr throws InternalError on LHS codegen nullptr",
          "[CodegenFunctions]") {
  ASTBinaryExpr binaryExpr("+", std::make_shared<nullcodegen::MockASTExpr>(),
                           std::make_shared<ASTInputExpr>());
  CodeGenContext ctx;
  CodeGenVisitor visitor;
  visitor.setContext(&ctx);
  REQUIRE_THROWS_AS(visitor.dispatch(&binaryExpr), InternalError);
}

TEST_CASE("CodegenFunction: ASTBinaryExpr throws InternalError on RHS codegen nullptr",
          "[CodegenFunctions]") {
  ASTBinaryExpr binaryExpr("+", std::make_shared<ASTInputExpr>(),
                           std::make_shared<nullcodegen::MockASTExpr>());
  CodeGenContext ctx;
  CodeGenVisitor visitor;
  visitor.setContext(&ctx);
  REQUIRE_THROWS_AS(visitor.dispatch(&binaryExpr), InternalError);
}

TEST_CASE("CodegenFunction: ASTBinaryExpr throws InternalError on bad OP",
          "[CodegenFunctions]") {
  REQUIRE_THROWS_AS(
      ASTBinaryExpr("ADDITION", std::make_shared<ASTInputExpr>(),
                    std::make_shared<ASTInputExpr>()),
      InternalError);
}

TEST_CASE("CodegenFunction: ASTOutputStmt throws InternalError on ARG codegen nullptr",
          "[CodegenFunctions]") {
  ASTOutputStmt outputStmt(std::make_shared<nullcodegen::MockASTExpr>());
  CodeGenContext ctx;
  CodeGenVisitor visitor;
  visitor.setContext(&ctx);
  REQUIRE_THROWS_AS(visitor.dispatch(&outputStmt), InternalError);
}

TEST_CASE(
    "CodegenFunction: ASTErrorStmt throws InternalError on ARG codegen nullptr",
    "[CodegenFunctions]") {
  ASTErrorStmt errorStmt(std::make_shared<nullcodegen::MockASTExpr>());
  CodeGenContext ctx;
  CodeGenVisitor visitor;
  visitor.setContext(&ctx);
  REQUIRE_THROWS_AS(visitor.dispatch(&errorStmt), InternalError);
}

TEST_CASE(
    "CodegenFunction: ASTVariableExpr throws InternalError on unknown NAME",
    "[CodegenFunctions]") {
  ASTVariableExpr variableExpr("foobar");
  CodeGenContext ctx;
  CodeGenVisitor visitor;
  visitor.setContext(&ctx);
  REQUIRE_THROWS_AS(visitor.dispatch(&variableExpr), InternalError);
}

TEST_CASE("CodegenFunction: ASTAllocExpr throws InternalError on INIT codegen nullptr",
          "[CodegenFunctions]") {
  ASTAllocExpr allocExpr(std::make_shared<nullcodegen::MockASTExpr>());
  CodeGenContext ctx;
  CodeGenVisitor visitor;
  visitor.setContext(&ctx);
  REQUIRE_THROWS_AS(visitor.dispatch(&allocExpr), InternalError);
}

TEST_CASE(
    "CodegenFunction: ASTRefExpr throws InternalError on VAR codegen nullptr",
    "[CodegenFunctions]") {
  ASTRefExpr refExpr(std::make_shared<nullcodegen::MockASTExpr>());
  CodeGenContext ctx;
  CodeGenVisitor visitor;
  visitor.setContext(&ctx);
  REQUIRE_THROWS_AS(visitor.dispatch(&refExpr), InternalError);
}

TEST_CASE(
    "CodegenFunction: ASTDeRefExpr throws InternalError on VAR codegen nullptr",
    "[CodegenFunctions]") {
  ASTDeRefExpr deRefExpr(std::make_shared<nullcodegen::MockASTExpr>());
  CodeGenContext ctx;
  CodeGenVisitor visitor;
  visitor.setContext(&ctx);
  REQUIRE_THROWS_AS(visitor.dispatch(&deRefExpr), InternalError);
}

TEST_CASE(
    "CodegenFunction: ASTAccessExpr throws InternalError on nonexistent field",
    "[CodegenFunctions]") {
  ASTAccessExpr accessExpr(std::make_shared<nullcodegen::MockASTExpr>(),
                           "foobar");
  CodeGenContext ctx;
  CodeGenVisitor visitor;
  visitor.setContext(&ctx);
  REQUIRE_THROWS_AS(visitor.dispatch(&accessExpr), InternalError);
}

TEST_CASE("CodegenFunction: ASTFunAppExpr throws InternalError on FUN codegen nullptr",
          "[CodegenFunctions]") {
  std::vector<std::shared_ptr<ASTExpr>> actuals;
  ASTFunAppExpr funAppExpr(std::make_shared<nullcodegen::MockASTExpr>(),
                           actuals);
  CodeGenContext ctx;
  CodeGenVisitor visitor;
  visitor.setContext(&ctx);
  REQUIRE_THROWS_AS(visitor.dispatch(&funAppExpr), InternalError);
}

#include "ASTHelper.h"

#include "antlr4-runtime.h"
#include <TOPLexer.h>
#include <TOPParser.h>

#include <catch2/catch_test_macros.hpp>

#include <iostream>

TEST_CASE("ASTBuilder: bad op string throws error", "[ASTBuilder]") {
  // Boilerplate just to setup a legitimate builder.
  std::stringstream stream;
  stream << R"(
    x = 1 + 1;
  )";
  antlr4::ANTLRInputStream input(stream);
  TOPLexer lexer(&input);
  antlr4::CommonTokenStream tokens(&lexer);
  TOPParser parser(&tokens);
  ASTBuilder tb(&parser);

  // Inject a bad operation token into an arbitrary binary expression.
  TOPParser::ExprContext exprContext;
  TOPParser::AdditiveExprContext context(&exprContext);
  antlr4::CommonToken mockToken(-1, "mock");
  context.op = &mockToken;

  REQUIRE_THROWS_AS(tb.visitAdditiveExpr(&context), std::runtime_error);
}

TEST_CASE("ASTBuilder: builds record and field access nodes", "[ASTBuilder]") {
  std::stringstream stream;
  stream << R"(
    main() {
      var r, x;
      r = {a:1, b:x};
      x = r.a;
      return x;
    }
  )";

  auto ast = ASTHelper::build_ast(stream);
  auto fun = ast->findFunctionByName("main");
  REQUIRE(fun != nullptr);

  auto stmts = fun->getStmts();
  REQUIRE(stmts.size() >= 3);

  auto firstAssign = dynamic_cast<ASTAssignStmt *>(stmts[0]);
  REQUIRE(firstAssign != nullptr);
  REQUIRE(dynamic_cast<ASTRecordExpr *>(firstAssign->getRHS()) != nullptr);

  auto secondAssign = dynamic_cast<ASTAssignStmt *>(stmts[1]);
  REQUIRE(secondAssign != nullptr);
  REQUIRE(dynamic_cast<ASTFieldAccessExpr *>(secondAssign->getRHS()) != nullptr);
}

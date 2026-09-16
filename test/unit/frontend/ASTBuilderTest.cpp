#include "ASTHelper.h"

#include "antlr4-runtime.h"
#include <TOPLexer.h>
#include <TOPParser.h>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

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

TEST_CASE("ASTBuilder: number literals are 64-bit", "[ASTBuilder]") {
  std::stringstream stream;
  stream << R"(
    main() {
      var a, b;
      a = 9223372036854775807;
      b = -9223372036854775808;
      return a;
    }
  )";
  auto ast = ASTHelper::build_ast(stream);
  struct NumberCollector : public ASTVisitor {
    std::vector<int64_t> values;
    void endVisit(ASTNumberExpr *element) override {
      values.push_back(element->getValue());
    }
  } collector;
  ast->accept(&collector);
  REQUIRE(collector.values ==
          std::vector<int64_t>{INT64_MAX, INT64_MIN});
}

TEST_CASE("ASTBuilder: out-of-range number literals are parse errors",
          "[ASTBuilder]") {
  std::stringstream positive;
  positive << "main() { return 9223372036854775808; }";
  REQUIRE_THROWS_WITH(ASTHelper::build_ast(positive),
                      Catch::Matchers::ContainsSubstring(
                          "out of range for a 64-bit int"));

  std::stringstream negative;
  negative << "main() { return -9223372036854775809; }";
  REQUIRE_THROWS_WITH(ASTHelper::build_ast(negative),
                      Catch::Matchers::ContainsSubstring(
                          "out of range for a 64-bit int"));
}

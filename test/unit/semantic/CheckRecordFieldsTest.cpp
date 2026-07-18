#include "CheckRecordFields.h"
#include "ASTHelper.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

TEST_CASE("Check Record Fields: unique names", "[Symbol]") {
  std::stringstream stream;
  stream << R"(ok() { var r; r = {a:1, b:2}; return 0; })";
  auto ast = ASTHelper::build_ast(stream);
  REQUIRE_NOTHROW(CheckRecordFields::check(ast.get()));
}

TEST_CASE("Check Record Fields: duplicate names rejected", "[Symbol]") {
  std::stringstream stream;
  stream << R"(bad() { var r; r = {a:1, a:2}; return 0; })";
  auto ast = ASTHelper::build_ast(stream);
  REQUIRE_THROWS_WITH(CheckRecordFields::check(ast.get()),
                      Catch::Matchers::ContainsSubstring("duplicate field 'a'"));
}

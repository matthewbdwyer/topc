#include "CheckAssignable.h"
#include "ASTHelper.h"
#include <catch2/matchers/catch_matchers_string.hpp>
#include "SemanticError.h"

#include <catch2/catch_test_macros.hpp>

#include <iostream>

TEST_CASE("Check Assignable: variable lhs", "[Symbol]") {
  std::stringstream stream;
  stream << R"(varlhs() { var x; x = 1; return 0; })";
  auto ast = ASTHelper::build_ast(stream);
  REQUIRE_NOTHROW(CheckAssignable::check(ast.get()));
}

TEST_CASE("Check Assignable: pointer lhs", "[Symbol]") {
  std::stringstream stream;
  stream << R"(ptrlhs() { var x; *x = 1; return 0; })";
  auto ast = ASTHelper::build_ast(stream);
  REQUIRE_NOTHROW(CheckAssignable::check(ast.get()));
}

TEST_CASE("Check Assignable: complex pointer lhs", "[Symbol]") {
  std::stringstream stream;
  stream
      << R"(foo(x) { return &x; } ptrlhs() { var x; *foo(&x) = 1; return 0; })";
  auto ast = ASTHelper::build_ast(stream);
  REQUIRE_NOTHROW(CheckAssignable::check(ast.get()));
}

TEST_CASE("Check Assignable: address of var", "[Symbol]") {
  std::stringstream stream;
  stream << R"(recordlhs() { var x, y; x = &y; return 0; })";
  auto ast = ASTHelper::build_ast(stream);
  REQUIRE_NOTHROW(CheckAssignable::check(ast.get()));
}

/************** the following are expected to fail the check ************/

TEST_CASE("Check Assignable: constant lhs", "[Symbol]") {
  std::stringstream stream;
  stream << R"(constlhs() { var x; 7 = x; return 0; })";
  auto ast = ASTHelper::build_ast(stream);
  REQUIRE_THROWS_WITH(CheckAssignable::check(ast.get()),
                         Catch::Matchers::ContainsSubstring("7 not an l-value"));
}

TEST_CASE("Check Assignable: binary lhs", "[Symbol]") {
  std::stringstream stream;
  stream << R"(binlhs() { var x; x+1 = x; return 0; })";
  auto ast = ASTHelper::build_ast(stream);
  REQUIRE_THROWS_WITH(CheckAssignable::check(ast.get()),
                         Catch::Matchers::ContainsSubstring("(x+1) not an l-value"));
}

TEST_CASE("Check Assignable: function lhs", "[Symbol]") {
  std::stringstream stream;
  stream << R"(foo() { return 0; } funlhs() { var x; foo() = x; return 0; })";
  auto ast = ASTHelper::build_ast(stream);
  REQUIRE_THROWS_WITH(CheckAssignable::check(ast.get()),
                         Catch::Matchers::ContainsSubstring("foo() not an l-value"));
}

TEST_CASE("Check Assignable: alloc lhs", "[Symbol]") {
  std::stringstream stream;
  stream << R"(alloclhs() { var x; alloc 1 = x; return 0; })";
  auto ast = ASTHelper::build_ast(stream);
  REQUIRE_THROWS_WITH(CheckAssignable::check(ast.get()),
                         Catch::Matchers::ContainsSubstring("alloc 1 not an l-value"));
}

TEST_CASE("Check Assignable: address of pointer", "[Symbol]") {
  std::stringstream stream;
  stream << R"(recordlhs(p) { var x; x = &(*p); return 0; })";
  auto ast = ASTHelper::build_ast(stream);
  REQUIRE_THROWS_WITH(CheckAssignable::check(ast.get()),
                         Catch::Matchers::ContainsSubstring("(*p) not an l-value"));
}

TEST_CASE("Check Assignable: address of expr", "[Symbol]") {
  std::stringstream stream;
  stream << R"(recordlhs(p) { var x, y; x = &(y*y); return 0; })";
  auto ast = ASTHelper::build_ast(stream);
  REQUIRE_THROWS_WITH(CheckAssignable::check(ast.get()),
                         Catch::Matchers::ContainsSubstring("(y*y) not an l-value"));
}

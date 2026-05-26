#include "SymbolTable.h"
#include "ASTHelper.h"
#include <catch2/matchers/catch_matchers_string.hpp>
#include "SemanticError.h"

#include <catch2/catch_test_macros.hpp>

#include <iostream>
#include <optional>

TEST_CASE("Symbol Table: locals", "[SymbolTable]") {
  std::stringstream stream;
  stream << R"(short() { var x, y, z; output x+y; return z; })";

  auto ast = ASTHelper::build_ast(stream);

  std::shared_ptr<SymbolTable> symbols;
  REQUIRE_NOTHROW(symbols = SymbolTable::build(ast.get()));

  std::stringstream outputStream;
  symbols->print(outputStream);
  std::string output = outputStream.str();

  std::size_t found = output.find("Functions : {short}");
  REQUIRE(found != std::string::npos);

  found = output.find("short : {x, y, z}");
  REQUIRE(found != std::string::npos);
}

TEST_CASE("Symbol Table: functions", "[SymbolTable]") {
  std::stringstream stream;
  stream << R"(foo() { return 0; } bar() { return 1; } baz() { return 2;})";

  auto ast = ASTHelper::build_ast(stream);

  std::shared_ptr<SymbolTable> symbols;
  REQUIRE_NOTHROW(symbols = SymbolTable::build(ast.get()));

  std::stringstream outputStream;
  symbols->print(outputStream);
  std::string output = outputStream.str();

  std::size_t found = output.find("Functions : {bar, baz, foo}");
  REQUIRE(found != std::string::npos);

  found = output.find("foo : {}");
  REQUIRE(found != std::string::npos);
}

TEST_CASE("Symbol Table: params", "[SymbolTable]") {
  std::stringstream stream;
  stream << R"(foo(a) { return a; } bar(a) { return a; } baz(b) { return b;})";

  auto ast = ASTHelper::build_ast(stream);

  std::shared_ptr<SymbolTable> symbols;
  REQUIRE_NOTHROW(symbols = SymbolTable::build(ast.get()));

  std::stringstream outputStream;
  symbols->print(outputStream);
  std::string output = outputStream.str();

  std::size_t found = output.find("Functions : {bar, baz, foo}");
  REQUIRE(found != std::string::npos);

  found = output.find("foo : {a}");
  REQUIRE(found != std::string::npos);

  found = output.find("bar : {a}");
  REQUIRE(found != std::string::npos);

  found = output.find("baz : {b}");
  REQUIRE(found != std::string::npos);
}

TEST_CASE("Symbol Table: locals params ", "[SymbolTable]") {
  std::stringstream stream;
  stream << R"(short(a, b, c) { var x, y, z; return z; })";

  auto ast = ASTHelper::build_ast(stream);

  std::shared_ptr<SymbolTable> symbols;
  REQUIRE_NOTHROW(symbols = SymbolTable::build(ast.get()));

  std::stringstream outputStream;
  symbols->print(outputStream);
  std::string output = outputStream.str();

  std::size_t found = output.find("Functions : {short}");
  REQUIRE(found != std::string::npos);

  found = output.find("short : {a, b, c, x, y, z}");
  REQUIRE(found != std::string::npos);
}

/******************** symbol errors *******************/

TEST_CASE("Symbol Table: local undeclared", "[SymbolTable]") {
  std::stringstream stream;
  stream << R"(short(a, b, c) { var x; return z; })";

  auto ast = ASTHelper::build_ast(stream);

  REQUIRE_THROWS_WITH(SymbolTable::build(ast.get()),
                         Catch::Matchers::ContainsSubstring("z undeclared"));
}

TEST_CASE("Symbol Table: locals param clash ", "[SymbolTable]") {
  std::stringstream stream;
  stream << R"(short(a, b, c) { var x, b, z; return z; })";

  auto ast = ASTHelper::build_ast(stream);

  REQUIRE_THROWS_WITH(SymbolTable::build(ast.get()),
                         Catch::Matchers::ContainsSubstring("b redeclared"));
}

TEST_CASE("Symbol Table: locals clash ", "[SymbolTable]") {
  std::stringstream stream;
  stream << R"(short(a, b, c) { var x, x, z; return z; })";

  auto ast = ASTHelper::build_ast(stream);

  REQUIRE_THROWS_WITH(SymbolTable::build(ast.get()),
                         Catch::Matchers::ContainsSubstring("x redeclared"));
}

TEST_CASE("Symbol Table: params clash ", "[SymbolTable]") {
  std::stringstream stream;
  stream << R"(short(a, b, a) { var x, y, z; return z; })";

  auto ast = ASTHelper::build_ast(stream);

  REQUIRE_THROWS_WITH(SymbolTable::build(ast.get()),
                         Catch::Matchers::ContainsSubstring("a redeclared"));
}

TEST_CASE("Symbol Table: functions clash ", "[SymbolTable]") {
  std::stringstream stream;
  stream << R"(foo() { return 0; } bar() { return 1; } foo() { return 2;})";

  auto ast = ASTHelper::build_ast(stream);

  REQUIRE_THROWS_WITH(SymbolTable::build(ast.get()),
                         Catch::Matchers::ContainsSubstring("foo already declared"));
}

TEST_CASE("Symbol Table: Unknown Function", "[SymbolTable]") {
  std::stringstream stream;
  stream << R"(short() { var x, y, z; output x+y; return z; })";
  auto ast = ASTHelper::build_ast(stream);
  std::shared_ptr<SymbolTable> symbols = SymbolTable::build(ast.get());
  REQUIRE(nullptr == symbols->getFunction("foo"));
}

// ============================================================
// Phase 5: sum type / constructor lookup
// ============================================================

TEST_CASE("Symbol Table: resolveConstructorName", "[SymbolTable]") {
  std::stringstream stream;
  stream << R"(
    type Option = Some(x) | None;
    main() { return 0; }
  )";
  auto ast = ASTHelper::build_ast(stream);
  std::shared_ptr<SymbolTable> symbols;
  REQUIRE_NOTHROW(symbols = SymbolTable::build(ast.get()));
  REQUIRE(symbols->getConstructor("Some") != nullptr);
  REQUIRE(symbols->getConstructor("None") != nullptr);
  REQUIRE(symbols->getConstructor("Foo") == nullptr);
}

TEST_CASE("Symbol Table: resolveTypeName", "[SymbolTable]") {
  std::stringstream stream;
  stream << R"(
    type Option = Some(x) | None;
    type Result = Ok(v) | Err(e);
    main() { return 0; }
  )";
  auto ast = ASTHelper::build_ast(stream);
  std::shared_ptr<SymbolTable> symbols;
  REQUIRE_NOTHROW(symbols = SymbolTable::build(ast.get()));
  REQUIRE(symbols->getSumType("Option") != nullptr);
  REQUIRE(symbols->getSumType("Result") != nullptr);
  REQUIRE(symbols->getSumType("Foo") == nullptr);
  // getSumTypes() returns all names
  auto names = symbols->getSumTypes();
  REQUIRE(names.size() == 2);
}

TEST_CASE("Symbol Table: case arm bindings scoped", "[SymbolTable]") {
  // Binding variable 'v' from a case arm must not be visible after the arm body.
  // Two sequential arms can reuse the same binding name without a symbol error.
  std::stringstream stream;
  stream << R"(
    type Option = Some(x) | None;
    main() {
      var o;
      case o of {
        Some(v) -> output v;
        None    -> output 0;
      }
      return 0;
    }
  )";
  auto ast = ASTHelper::build_ast(stream);
  REQUIRE_NOTHROW(SymbolTable::build(ast.get()));
}

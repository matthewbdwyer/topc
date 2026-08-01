#include "ASTVisualizer.h"
#include "ASTHelper.h"
#include "GeneralHelper.h"
#include "Iterator.h"
#include "SyntaxTree.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <iostream>

TEST_CASE("ASTVisualizer: Generate dot graph", "[ASTVisualizer]") {
  std::stringstream stream;
  stream << R"(
      short() {
        var x, y, z;
        x = input;
        y = alloc x;
        *y = x;
        z = *y;
        return z;
      }
    )";

  std::shared_ptr<ASTProgram> ast = std::move(ASTHelper::build_ast(stream));
  SyntaxTree syntaxTree(ast);

  std::stringstream graph;
  ASTVisualizer visualizer(graph);
  visualizer.buildGraph(syntaxTree);

  int expectedNodeCount = 24;
  int expectedEdgeCount = 23;
  REQUIRE(expectedNodeCount ==
          GeneralHelper::countSubstrings(graph.str(), "label"));
  REQUIRE(expectedEdgeCount ==
          GeneralHelper::countSubstrings(graph.str(), "->"));
}

TEST_CASE("ASTVisualizer: Generate ascii tree", "[ASTVisualizer]") {
  std::stringstream stream;
  stream << R"(
      short() {
        var x;
        x = input;
        return x;
      }
    )";

  std::shared_ptr<ASTProgram> ast = std::move(ASTHelper::build_ast(stream));
  SyntaxTree syntaxTree(ast);

  std::stringstream graph;
  ASTVisualizer visualizer(graph);
  visualizer.buildAscii(syntaxTree);

  std::string out = graph.str();
  REQUIRE(GeneralHelper::countSubstrings(out, "Program") == 1);
  REQUIRE(GeneralHelper::countSubstrings(out, "Function: short") == 1);
  REQUIRE(GeneralHelper::countSubstrings(out, "ReturnStmt") == 1);
}

TEST_CASE("ASTVisualizer: ASCII labels nested case structure",
          "[ASTVisualizer]") {
  std::stringstream stream;
  stream << R"(
      type Inner = Lit(value) | Neg(value);
      type Outer = Empty | Wrap(inner);
      evaluate(outer) {
        var result;
        case outer of {
          Empty -> result = 0;
          Wrap(Lit(n)) -> result = n;
          Wrap(Neg(n)) -> result = 0 - n;
        }
        return result;
      }
    )";

  auto ast = ASTHelper::build_ast(stream);
  SyntaxTree syntaxTree(ast);
  std::stringstream graph;
  ASTVisualizer visualizer(graph);
  visualizer.buildAscii(syntaxTree);

  const auto out = graph.str();
  using Catch::Matchers::ContainsSubstring;
  REQUIRE_THAT(out,
               ContainsSubstring("├── case-expression: VariableExpr: outer"));
  REQUIRE_THAT(out, ContainsSubstring("├── arm[1]: CaseArm"));
  REQUIRE_THAT(out,
               ContainsSubstring("├── pattern: ConstructorPattern: Wrap"));
  REQUIRE_THAT(out,
               ContainsSubstring("└── payload[0]: ConstructorPattern: Lit"));
  REQUIRE_THAT(out,
               ContainsSubstring("└── payload[0]: BindingPattern: n"));
  REQUIRE_THAT(out, ContainsSubstring("└── body: AssignStmt"));
  REQUIRE_THAT(out, ContainsSubstring("├── lhs: VariableExpr: result"));
  REQUIRE_THAT(out, ContainsSubstring("└── rhs: VariableExpr: n"));
}

#include "ASTVisualizer.h"
#include "ASTHelper.h"
#include "GeneralHelper.h"
#include "Iterator.h"
#include "SyntaxTree.h"

#include <catch2/catch_test_macros.hpp>

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
  REQUIRE(GeneralHelper::countSubstrings(out, "start") == 1);
  REQUIRE(GeneralHelper::countSubstrings(out, "short() {...}") == 1);
  REQUIRE(GeneralHelper::countSubstrings(out, "return x;") == 1);
}

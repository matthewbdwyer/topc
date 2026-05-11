/*! \file ASTNodeNoLLVMTest.cpp
 *  \brief Verify that AST node types can be constructed and traversed
 *         without including any LLVM header (Phase 9 complete).
 */

#include "ASTHelper.h"
#include "ASTVisitor.h"
#include "ASTBinaryExpr.h"
#include "ASTVariableExpr.h"
#include "ASTDeclNode.h"
#include "ASTFunction.h"
#include "ASTProgram.h"

#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Minimal visitor that counts visited nodes — no LLVM needed.
// ---------------------------------------------------------------------------
class CountingVisitor : public ASTVisitor {
public:
  int count = 0;
  bool visit(ASTBinaryExpr *) override { ++count; return true; }
  bool visit(ASTVariableExpr *) override { ++count; return true; }
  bool visit(ASTDeclNode *) override { ++count; return true; }
  bool visit(ASTFunction *) override { ++count; return true; }
  bool visit(ASTProgram *) override { ++count; return true; }
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_CASE("ASTNodeNoLLVM: ASTBinaryExpr can be constructed without LLVM context",
          "[ASTNodeNoLLVM]") {
  std::stringstream stream;
  stream << R"(
    foo() {
      var x;
      x = 1 + 2;
      return x;
    }
  )";
  auto ast = ASTHelper::build_ast(stream);
  REQUIRE(ast != nullptr);
  auto binExpr = ASTHelper::find_node<ASTBinaryExpr>(ast);
  REQUIRE(binExpr != nullptr);
}

TEST_CASE("ASTNodeNoLLVM: ASTVariableExpr can be constructed without LLVM context",
          "[ASTNodeNoLLVM]") {
  std::stringstream stream;
  stream << R"(
    foo() {
      var x;
      return x;
    }
  )";
  auto ast = ASTHelper::build_ast(stream);
  REQUIRE(ast != nullptr);
  auto varExpr = ASTHelper::find_node<ASTVariableExpr>(ast);
  REQUIRE(varExpr != nullptr);
  REQUIRE(varExpr->getName() == "x");
}

TEST_CASE("ASTNodeNoLLVM: ASTDeclNode can be constructed without LLVM context",
          "[ASTNodeNoLLVM]") {
  std::stringstream stream;
  stream << R"(
    foo() {
      var x;
      return x;
    }
  )";
  auto ast = ASTHelper::build_ast(stream);
  REQUIRE(ast != nullptr);
  auto decl = ASTHelper::find_node<ASTDeclNode>(ast);
  REQUIRE(decl != nullptr);
  REQUIRE(decl->getName() == "x");
}

TEST_CASE("ASTNodeNoLLVM: ASTFunction can be constructed without LLVM context",
          "[ASTNodeNoLLVM]") {
  std::stringstream stream;
  stream << R"(
    foo(a, b) {
      return a + b;
    }
  )";
  auto ast = ASTHelper::build_ast(stream);
  REQUIRE(ast != nullptr);
  auto fn = ASTHelper::find_node<ASTFunction>(ast);
  REQUIRE(fn != nullptr);
  REQUIRE(fn->getName() == "foo");
}

TEST_CASE("ASTNodeNoLLVM: ASTProgram can be traversed with a visitor without LLVM",
          "[ASTNodeNoLLVM]") {
  std::stringstream stream;
  stream << R"(
    foo(a, b) {
      var x;
      x = a + b;
      return x;
    }
  )";
  auto ast = ASTHelper::build_ast(stream);
  REQUIRE(ast != nullptr);

  CountingVisitor v;
  ast->accept(&v);
  // At minimum we should have visited the program and at least one function
  REQUIRE(v.count >= 2);
}

#include "ASTDestroyStmt.h"
#include "ASTFunction.h"
#include "ASTHelper.h"
#include "SemanticAnalysis.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <sstream>

namespace {
std::shared_ptr<ASTProgram> parseProgram(const char *source) {
  std::stringstream stream;
  stream << source;
  return ASTHelper::build_ast(stream);
}

bool hasDestroyStmt(ASTFunction *function) {
  for (auto *stmt : function->getStmts()) {
    if (dynamic_cast<ASTDestroyStmt *>(stmt) != nullptr) {
      return true;
    }
  }
  return false;
}
} // namespace

TEST_CASE("SemanticAnalysis: retains IntraproceduralCFGs",
          "[SemanticAnalysis][CFG]") {
  auto ast = parseProgram(R"(
    main() {
      var x;
      x = 1;
      return x;
    }
  )");

  auto analysis = SemanticAnalysis::analyze(ast.get());
  REQUIRE(analysis != nullptr);
  REQUIRE(analysis->getIntraproceduralCFGs() != nullptr);

  auto cfgs = analysis->getIntraproceduralCFGs()->getAll();
  REQUIRE(cfgs.size() == ast->getFunctions().size());
}

TEST_CASE("SemanticAnalysis: CFGs are available with call graph and type results",
          "[SemanticAnalysis][CFG]") {
  auto ast = parseProgram(R"(
    id(x) {
      return x;
    }

    main() {
      var y;
      y = id(1);
      return y;
    }
  )");

  auto analysis = SemanticAnalysis::analyze(ast.get());
  REQUIRE(analysis->getIntraproceduralCFGs() != nullptr);
  REQUIRE(analysis->getCallGraph() != nullptr);
  REQUIRE(analysis->getTypeResults() != nullptr);
}

TEST_CASE("SemanticAnalysis: CFG construction precedes destruction insertion",
          "[SemanticAnalysis][CFG]") {
  auto ast = parseProgram(R"(
    type Flag = On | Off;

    main() {
      var p;
      p = alloc 5;
      return 0;
    }
  )");

  auto *mainFn = ast->findFunctionByName("main");
  REQUIRE(mainFn != nullptr);
  REQUIRE_FALSE(hasDestroyStmt(mainFn));

  auto analysis = SemanticAnalysis::analyze(ast.get());
  REQUIRE(analysis->getIntraproceduralCFGs() != nullptr);

  const auto &cfg =
      analysis->getIntraproceduralCFGs()->get(mainFn);
  REQUIRE_NOTHROW(cfg.validate());

  // The post-semantic AST should include inserted destroys.
  REQUIRE(hasDestroyStmt(mainFn));

  // The source CFG should remain destroy-free.
  for (const auto &block : cfg.getBlocks()) {
    for (auto *stmt : block.getStatements()) {
      REQUIRE(dynamic_cast<const ASTDestroyStmt *>(stmt) == nullptr);
    }
    REQUIRE(dynamic_cast<const ASTDestroyStmt *>(
                block.getTerminatorStatement()) == nullptr);
  }
}

TEST_CASE("SemanticAnalysis: existing move diagnostics remain unchanged",
          "[SemanticAnalysis][CFG]") {
  auto ast = parseProgram(R"(
    type Flag = On | Off;

    main() {
      var p, q;
      p = alloc 5;
      q = p;
      output *p;
      return 0;
    }
  )");

  REQUIRE_THROWS_WITH(SemanticAnalysis::analyze(ast.get()),
                      Catch::Matchers::ContainsSubstring("used after move"));
}

TEST_CASE("SemanticAnalysis: existing borrow diagnostics remain unchanged",
          "[SemanticAnalysis][CFG]") {
  auto ast = parseProgram(R"(
    type Flag = On | Off;

    main() {
      var p, b;
      p = alloc 5;
      b = &p;
      return 0;
    }
  )");

  REQUIRE_THROWS_WITH(
      SemanticAnalysis::analyze(ast.get()),
      Catch::Matchers::ContainsSubstring("immediate function argument"));
}

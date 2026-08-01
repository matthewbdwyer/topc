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

bool destroysVar(ASTFunction *function, const std::string &name) {
  for (auto *stmt : function->getStmts()) {
    auto *destroy = dynamic_cast<ASTDestroyStmt *>(stmt);
    if (destroy != nullptr && destroy->getVar()->getName() == name) {
      return true;
    }
  }
  return false;
}

std::size_t destroyCount(ASTFunction *function) {
  std::size_t count = 0;
  for (auto *stmt : function->getStmts()) {
    if (dynamic_cast<ASTDestroyStmt *>(stmt) != nullptr) {
      ++count;
    }
  }
  return count;
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

TEST_CASE("SemanticAnalysis: fresh-own call through borrow actual is destroyed",
          "[SemanticAnalysis][DestructionPass]") {
  auto ast = parseProgram(R"(
    type Flag = On | Off;

    mk(seed) {
      return alloc 1;
    }

    main() {
      var x, p;
      x = 1;
      p = mk(&x);
      return 0;
    }
  )");

  auto *mainFn = ast->findFunctionByName("main");
  REQUIRE(mainFn != nullptr);
  REQUIRE_FALSE(destroysVar(mainFn, "p"));

  auto analysis = SemanticAnalysis::analyze(ast.get());
  REQUIRE(analysis != nullptr);
  REQUIRE(destroysVar(mainFn, "p"));
}

TEST_CASE("SemanticAnalysis: polymorphic identity destroys returned owner",
          "[SemanticAnalysis][FunctionEffectSummaries][DestructionPass]"
          "[ReferenceMode]") {
  auto ast = parseProgram(R"(
    identity(value) {
      return value;
    }
    main() {
      var first, second;
      first = alloc 42;
      second = identity(first);
      return *second;
    }
  )");

  auto *identity = ast->findFunctionByName("identity");
  auto *mainFn = ast->findFunctionByName("main");
  REQUIRE(identity != nullptr);
  REQUIRE(mainFn != nullptr);

  auto analysis = SemanticAnalysis::analyze(ast.get());
  auto *summary =
      analysis->getFunctionEffectSummaries()->get(identity->getDecl());
  REQUIRE(summary != nullptr);
  REQUIRE(summary->formalModes.size() == 1);
  REQUIRE(summary->formalModes[0] ==
          FunctionEffectSummaries::FormalMode::DependsOnInstantiation);
  REQUIRE(summary->returnOrigin ==
          FunctionEffectSummaries::ReturnOrigin::FromFormal);
  REQUIRE(summary->returnFormalIndex == 0);
  REQUIRE_FALSE(destroysVar(mainFn, "first"));
  REQUIRE(destroysVar(mainFn, "second"));
  REQUIRE(destroyCount(mainFn) == 1);
}

TEST_CASE("SemanticAnalysis: summary preserves matching branch return origin",
          "[SemanticAnalysis][FunctionEffectSummaries]") {
  auto ast = parseProgram(R"(
    type Flag = On | Off;

    choose(c, x) {
      var r;
      if (c) r = x; else r = x;
      return r;
    }

    main() {
      return 0;
    }
  )");

  auto analysis = SemanticAnalysis::analyze(ast.get());
  auto *chooseFn = ast->findFunctionByName("choose");
  REQUIRE(chooseFn != nullptr);

  auto *summary =
      analysis->getFunctionEffectSummaries()->get(chooseFn->getDecl());
  REQUIRE(summary != nullptr);
  REQUIRE(summary->returnOrigin ==
          FunctionEffectSummaries::ReturnOrigin::FromFormal);
  REQUIRE(summary->returnFormalIndex == 1);
}

TEST_CASE("SemanticAnalysis: summary marks conflicting branch origins unknown",
          "[SemanticAnalysis][FunctionEffectSummaries]") {
  auto ast = parseProgram(R"(
    type Flag = On | Off;

    choose(c, x, y) {
      var r;
      if (c) r = x; else r = y;
      return r;
    }

    main() {
      return 0;
    }
  )");

  auto analysis = SemanticAnalysis::analyze(ast.get());
  auto *chooseFn = ast->findFunctionByName("choose");
  REQUIRE(chooseFn != nullptr);

  auto *summary =
      analysis->getFunctionEffectSummaries()->get(chooseFn->getDecl());
  REQUIRE(summary != nullptr);
  REQUIRE(summary->returnOrigin ==
          FunctionEffectSummaries::ReturnOrigin::Unknown);
  REQUIRE(summary->returnFormalIndex == -1);
}

TEST_CASE("SemanticAnalysis: recursive function type reports unsupported ownership analysis",
          "[SemanticAnalysis][FunctionEffectSummaries]") {
  auto ast = parseProgram(R"(
    foo(n, f) {
      var r;
      if (n == 0) {
        r = 1;
      } else {
        r = n * f(n - 1, f);
      }
      return r;
    }

    main() {
      return foo(3, foo);
    }
  )");

  REQUIRE_THROWS_WITH(
      SemanticAnalysis::analyze(ast.get()),
      Catch::Matchers::ContainsSubstring(
          "recursive types are not yet supported in ownership analysis"));
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

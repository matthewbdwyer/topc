#include "AST.h"
#include "ASTHelper.h"
#include "ASTNodeHelpers.h"
#include "CodeGenContext.h"
#include "CodeGenerator.h"
#include "CodeGenVisitor.h"
#include "InternalError.h"
#include "ParserHelper.h"
#include "SemanticAnalysis.h"

#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>

// ---------------------------------------------------------------------------
// Shared compile helper (also used in CodegenStateIsolationTest.cpp pattern).
// ---------------------------------------------------------------------------
namespace {
std::shared_ptr<llvm::Module> compileModule(const std::string &src) {
  std::stringstream ss(src);
  auto ast      = ASTHelper::build_ast(ss);
  auto analysis = SemanticAnalysis::analyze(ast.get());
  return CodeGenerator::generate(ast.get(), analysis.get(), "test");
}
} // namespace

TEST_CASE("CodegenFunction: ASTDeclNode throws InternalError on codegen",
          "[CodegenFunctions]") {
  ASTDeclNode node("foo");
  CodeGenContext ctx;
  CodeGenVisitor visitor;
  visitor.setContext(&ctx);
  REQUIRE_THROWS_AS(visitor.dispatch(&node), InternalError);
}

TEST_CASE("CodegenFunction: ASTAssignsStmt throws InternalError on LHS codegen nullptr",
          "[CodegenFunctions]") {
  ASTAssignStmt assignStmt(std::make_shared<nullcodegen::MockASTExpr>(),
                           std::make_shared<ASTInputExpr>());
  CodeGenContext ctx;
  CodeGenVisitor visitor;
  visitor.setContext(&ctx);
  REQUIRE_THROWS_AS(visitor.dispatch(&assignStmt), InternalError);
}

TEST_CASE("CodegenFunction: ASTAssignsStmt throws InternalError on RHS codegen nullptr",
          "[CodegenFunctions]") {
  ASTAssignStmt assignStmt(std::make_shared<ASTInputExpr>(),
                           std::make_shared<nullcodegen::MockASTExpr>());
  CodeGenContext ctx;
  CodeGenVisitor visitor;
  visitor.setContext(&ctx);
  REQUIRE_THROWS_AS(visitor.dispatch(&assignStmt), InternalError);
}

TEST_CASE(
    "CodegenFunction: ASTIfStmt throws InternalError on COND codegen nullptr",
    "[CodegenFunctions]") {
  ASTIfStmt ifStmt(
      std::make_shared<nullcodegen::MockASTExpr>(),
      std::make_shared<ASTReturnStmt>(std::make_shared<ASTNumberExpr>(42)),
      std::make_shared<ASTReturnStmt>(std::make_shared<ASTNumberExpr>(42)));
  CodeGenContext ctx;
  CodeGenVisitor visitor;
  visitor.setContext(&ctx);
  REQUIRE_THROWS_AS(visitor.dispatch(&ifStmt), InternalError);
}

TEST_CASE("CodegenFunction: ASTBinaryExpr throws InternalError on LHS codegen nullptr",
          "[CodegenFunctions]") {
  ASTBinaryExpr binaryExpr("+", std::make_shared<nullcodegen::MockASTExpr>(),
                           std::make_shared<ASTInputExpr>());
  CodeGenContext ctx;
  CodeGenVisitor visitor;
  visitor.setContext(&ctx);
  REQUIRE_THROWS_AS(visitor.dispatch(&binaryExpr), InternalError);
}

TEST_CASE("CodegenFunction: ASTBinaryExpr throws InternalError on RHS codegen nullptr",
          "[CodegenFunctions]") {
  ASTBinaryExpr binaryExpr("+", std::make_shared<ASTInputExpr>(),
                           std::make_shared<nullcodegen::MockASTExpr>());
  CodeGenContext ctx;
  CodeGenVisitor visitor;
  visitor.setContext(&ctx);
  REQUIRE_THROWS_AS(visitor.dispatch(&binaryExpr), InternalError);
}

TEST_CASE("CodegenFunction: ASTBinaryExpr throws InternalError on bad OP",
          "[CodegenFunctions]") {
  REQUIRE_THROWS_AS(
      ASTBinaryExpr("ADDITION", std::make_shared<ASTInputExpr>(),
                    std::make_shared<ASTInputExpr>()),
      InternalError);
}

TEST_CASE("CodegenFunction: ASTOutputStmt throws InternalError on ARG codegen nullptr",
          "[CodegenFunctions]") {
  ASTOutputStmt outputStmt(std::make_shared<nullcodegen::MockASTExpr>());
  CodeGenContext ctx;
  CodeGenVisitor visitor;
  visitor.setContext(&ctx);
  REQUIRE_THROWS_AS(visitor.dispatch(&outputStmt), InternalError);
}

TEST_CASE(
    "CodegenFunction: ASTErrorStmt throws InternalError on ARG codegen nullptr",
    "[CodegenFunctions]") {
  ASTErrorStmt errorStmt(std::make_shared<nullcodegen::MockASTExpr>());
  CodeGenContext ctx;
  CodeGenVisitor visitor;
  visitor.setContext(&ctx);
  REQUIRE_THROWS_AS(visitor.dispatch(&errorStmt), InternalError);
}

TEST_CASE(
    "CodegenFunction: ASTVariableExpr throws InternalError on unknown NAME",
    "[CodegenFunctions]") {
  ASTVariableExpr variableExpr("foobar");
  CodeGenContext ctx;
  CodeGenVisitor visitor;
  visitor.setContext(&ctx);
  REQUIRE_THROWS_AS(visitor.dispatch(&variableExpr), InternalError);
}

TEST_CASE("CodegenFunction: ASTAllocExpr throws InternalError on INIT codegen nullptr",
          "[CodegenFunctions]") {
  ASTAllocExpr allocExpr(std::make_shared<nullcodegen::MockASTExpr>());
  CodeGenContext ctx;
  CodeGenVisitor visitor;
  visitor.setContext(&ctx);
  REQUIRE_THROWS_AS(visitor.dispatch(&allocExpr), InternalError);
}

TEST_CASE(
    "CodegenFunction: borrow expression throws InternalError on VAR codegen nullptr",
    "[CodegenFunctions]") {
  ASTBorrowExpr refExpr(std::make_shared<nullcodegen::MockASTExpr>());
  CodeGenContext ctx;
  CodeGenVisitor visitor;
  visitor.setContext(&ctx);
  REQUIRE_THROWS_AS(visitor.dispatch(&refExpr), InternalError);
}

TEST_CASE(
    "CodegenFunction: ASTDeRefExpr throws InternalError on VAR codegen nullptr",
    "[CodegenFunctions]") {
  ASTDeRefExpr deRefExpr(std::make_shared<nullcodegen::MockASTExpr>());
  CodeGenContext ctx;
  CodeGenVisitor visitor;
  visitor.setContext(&ctx);
  REQUIRE_THROWS_AS(visitor.dispatch(&deRefExpr), InternalError);
}

TEST_CASE("CodegenFunction: ASTFunAppExpr throws InternalError on FUN codegen nullptr",
          "[CodegenFunctions]") {
  std::vector<std::shared_ptr<ASTExpr>> actuals;
  ASTFunAppExpr funAppExpr(std::make_shared<nullcodegen::MockASTExpr>(),
                           actuals);
  CodeGenContext ctx;
  CodeGenVisitor visitor;
  visitor.setContext(&ctx);
  REQUIRE_THROWS_AS(visitor.dispatch(&funAppExpr), InternalError);
}

// ---------------------------------------------------------------------------
// Phase B4 codegen pattern tests
// ---------------------------------------------------------------------------

TEST_CASE("CodegenFunctions: wildcard pattern generates no named-value store for _",
          "[CodegenFunctions][B4]") {
  // Some(_) pattern: the wildcard should not create a binding named '_'.
  static const char *src = R"(
    type MaybeInt = None | Some(val);
    f(x) {
      var r; r = 0;
      case x of {
        None    -> r = 0;
        Some(_) -> r = 1;
      }
      return r;
    }
    main() { return 1 - f(Some(42)); }
  )";
  auto mod = compileModule(src);
  REQUIRE(mod != nullptr);

  // The IR must not contain an alloca named '_' (wildcard should not be bound).
  std::string ir;
  llvm::raw_string_ostream oss(ir);
  mod->print(oss, nullptr);
  REQUIRE(ir.find("alloca") != std::string::npos); // sanity: allocas exist
  REQUIRE(ir.find("\"_\"") == std::string::npos);  // no alloca named _
}


TEST_CASE("CodegenFunctions: nested ctor pattern generates inner tag comparison",
          "[CodegenFunctions][B4]") {
  static const char *src = R"(
    type Inner = Lit(n) | Neg(x);
    type Outer = Empty | Wrap(inner);
    innerVal(o) {
      var r; r = 0;
      case o of {
        Empty        -> r = -1;
        Wrap(Lit(x)) -> r = x;
        Wrap(Neg(y)) -> r = 0 - y;
      }
      return r;
    }
    main() {
      var ok; ok = 1;
      if (innerVal(Empty) != -1)        { ok = 0; }
      if (innerVal(Wrap(Lit(5))) != 5)  { ok = 0; }
      if (innerVal(Wrap(Neg(3))) != -3) { ok = 0; }
      return 1 - ok;
    }
  )";
  auto mod = compileModule(src);
  REQUIRE(mod != nullptr);

  std::string ir;
  llvm::raw_string_ostream oss(ir);
  mod->print(oss, nullptr);

  // Two Wrap arms with different inner ctors → at least two ICmpEQ instructions
  // (one per inner tag check).
  std::size_t cmpCount = 0;
  std::size_t pos = 0;
  while ((pos = ir.find("icmp eq", pos)) != std::string::npos) {
    ++cmpCount;
    pos += 7;
  }
  REQUIRE(cmpCount >= 2);
}

TEST_CASE("CodegenFunctions: wildcard on Own payload compiles and produces call to free",
          "[CodegenFunctions][B4]") {
  static const char *src = R"(
    type MaybeAlloc = Empty | Hold(ref);
    f(x) {
      var r; r = 0;
      case x of {
        Empty   -> r = 0;
        Hold(_) -> r = 1;
      }
      return r;
    }
    main() { return 1 - f(Hold(alloc 5)); }
  )";
  auto mod = compileModule(src);
  REQUIRE(mod != nullptr);

  std::string ir;
  llvm::raw_string_ostream oss(ir);
  mod->print(oss, nullptr);

  // The wildcard at an Own position should emit a call to free.
  REQUIRE(ir.find("@free") != std::string::npos);
}

TEST_CASE("CodegenFunctions: polymorphic identity preserves owner cleanup",
          "[CodegenFunctions][ReferenceMode]") {
  auto mod = compileModule(R"(
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
  REQUIRE(mod != nullptr);

  std::string ir;
  llvm::raw_string_ostream output(ir);
  mod->print(output, nullptr);

  auto countCalls = [&ir](const std::string &callee) {
    std::size_t count = 0;
    std::size_t position = 0;
    while ((position = ir.find(callee, position)) != std::string::npos) {
      ++count;
      position += callee.size();
    }
    return count;
  };

  REQUIRE(countCalls("call ptr @calloc") == 1);
  REQUIRE(countCalls("call void @free") == 1);
}


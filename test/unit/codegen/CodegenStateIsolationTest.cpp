#include "ASTHelper.h"
#include "CodeGenContext.h"
#include "CodeGenerator.h"
#include "SemanticAnalysis.h"

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>

// Helper: parse, analyze, and codegen a program string. Returns the LLVM module.
static std::shared_ptr<llvm::Module> compile(const std::string &src) {
  std::stringstream ss(src);
  auto ast = ASTHelper::build_ast(ss);
  auto analysis = SemanticAnalysis::analyze(ast.get(), false);
  return CodeGenerator::generate(ast.get(), analysis.get(), "test_prog");
}

// Minimal well-typed programs used across sections.
static const char *progA = R"(
  f(x) { return x + 1; }
  main() { var r; r = f(0); return 0; }
)";

static const char *progB = R"(
  main() { return 42; }
)";

static const char *progWithLoop = R"(
  main() {
    var i;
    i = 0;
    while (i > 0) { i = i - 1; }
    return 0;
  }
)";

TEST_CASE("CodegenIsolation: single compilation produces a valid module",
          "[CodegenIsolation]") {
  auto mod = compile(progA);
  REQUIRE(mod != nullptr);
  // TIP `main` is compiled to `_tip_main` in LLVM; `f` keeps its own name.
  REQUIRE(mod->getFunction("_tip_main") != nullptr);
  REQUIRE(mod->getFunction("f")         != nullptr);
}

TEST_CASE("CodegenIsolation: second compilation after first produces correct module",
          "[CodegenIsolation]") {
  // Compile program A first (to populate global maps with f, main).
  auto modA = compile(progA);
  REQUIRE(modA != nullptr);

  // Compile program B (only main). Verify B's module is correct despite A's
  // stale entries remaining in the file-scope globals (functionIndex etc.).
  auto modB = compile(progB);
  REQUIRE(modB != nullptr);

  // B should contain _tip_main but not f.
  REQUIRE(modB->getFunction("_tip_main") != nullptr);
  REQUIRE(modB->getFunction("f")         == nullptr);
}

TEST_CASE("CodegenIsolation: same program compiled twice produces equivalent modules",
          "[CodegenIsolation]") {
  auto mod1 = compile(progWithLoop);
  REQUIRE(mod1 != nullptr);

  auto mod2 = compile(progWithLoop);
  REQUIRE(mod2 != nullptr);

  // Both modules should have main and the label numbering should be consistent.
  REQUIRE(mod1->getFunction("_tip_main") != nullptr);
  REQUIRE(mod2->getFunction("_tip_main") != nullptr);

  // Dump both to string and compare. If any global state bleeds between
  // compilations (e.g., label counters not reset), the IR text will differ.
  std::string ir1, ir2;
  llvm::raw_string_ostream out1(ir1), out2(ir2);
  mod1->print(out1, nullptr);
  mod2->print(out2, nullptr);
  // Strip the module identifiers (they include a per-compile string) before comparing.
  auto strip_name = [](std::string &s) {
    auto start = s.find("source_filename");
    auto end   = s.find('\n', start);
    if (start != std::string::npos && end != std::string::npos)
      s.replace(start, end - start, "source_filename = \"\"");
  };
  strip_name(ir1);
  strip_name(ir2);
  REQUIRE(ir1 == ir2);
}

// ---------------------------------------------------------------------------
// Phase 8: struct isolation test.
//
// Two distinct CodeGenContext instances must be completely independent.
// ---------------------------------------------------------------------------
TEST_CASE("CodegenIsolation: [concept] two CodeGenContext instances are independent",
          "[CodegenIsolation]") {
  CodeGenContext ctxA, ctxB;
  ctxA.labelNum = 7;
  REQUIRE(ctxB.labelNum == 0);

  ctxA.lValueGen = true;
  REQUIRE(!ctxB.lValueGen);
}

#include "cfg/CFGRenderer.h"

#include "ASTHelper.h"
#include "cfg/IntraproceduralCFGs.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <sstream>
#include <string>

namespace {
std::shared_ptr<ASTProgram> parseProgram(const char *source) {
  std::stringstream program;
  program << source;
  return ASTHelper::build_ast(program);
}
} // namespace

TEST_CASE("CFGRenderer: ASCII includes function header and terminal nodes",
          "[cfg][CFGRenderer]") {
  auto ast = parseProgram(R"(
    main() {
      return 0;
    }
  )");

  auto cfgs = IntraproceduralCFGs::build(ast.get());
  auto *mainFunction = ast->findFunctionByName("main");
  REQUIRE(mainFunction != nullptr);

  std::ostringstream out;
  CFGRenderer::renderAscii(cfgs->get(mainFunction), out);
  const auto text = out.str();

  REQUIRE_THAT(text, Catch::Matchers::ContainsSubstring("[cfg main]"));
  REQUIRE_THAT(text, Catch::Matchers::ContainsSubstring("entry ->"));
  REQUIRE_THAT(text, Catch::Matchers::ContainsSubstring("exit"));
}

TEST_CASE("CFGRenderer: DOT includes digraph and directed edges",
          "[cfg][CFGRenderer]") {
  auto ast = parseProgram(R"(
    main() {
      var x;
      x = 1;
      return x;
    }
  )");

  auto cfgs = IntraproceduralCFGs::build(ast.get());
  auto *mainFunction = ast->findFunctionByName("main");
  REQUIRE(mainFunction != nullptr);

  std::ostringstream out;
  CFGRenderer::renderDot(cfgs->get(mainFunction), out);
  const auto text = out.str();

  REQUIRE_THAT(text, Catch::Matchers::ContainsSubstring("digraph \"main\""));
  REQUIRE_THAT(text, Catch::Matchers::ContainsSubstring("entry [label=\"entry\"]"));
  REQUIRE_THAT(text, Catch::Matchers::ContainsSubstring("exit [label=\"exit\"]"));
  REQUIRE_THAT(text, Catch::Matchers::ContainsSubstring("entry ->"));
}

TEST_CASE("CFGRenderer: conditional edge order is deterministic",
          "[cfg][CFGRenderer]") {
  auto ast = parseProgram(R"(
    main() {
      var x;
      x = 1;
      if (x) {
        output x;
      } else {
        output x;
      }
      return x;
    }
  )");

  auto cfgs = IntraproceduralCFGs::build(ast.get());
  auto *mainFunction = ast->findFunctionByName("main");
  REQUIRE(mainFunction != nullptr);

  std::ostringstream ascii;
  CFGRenderer::renderAscii(cfgs->get(mainFunction), ascii);
  const auto asciiText = ascii.str();

  const auto truePos = asciiText.find("true ->");
  const auto falsePos = asciiText.find("false ->");
  REQUIRE(truePos != std::string::npos);
  REQUIRE(falsePos != std::string::npos);
  REQUIRE(truePos < falsePos);

  std::ostringstream dot;
  CFGRenderer::renderDot(cfgs->get(mainFunction), dot);
  const auto dotText = dot.str();

  const auto dotTruePos = dotText.find("[label=\"true\"]");
  const auto dotFalsePos = dotText.find("[label=\"false\"]");
  REQUIRE(dotTruePos != std::string::npos);
  REQUIRE(dotFalsePos != std::string::npos);
  REQUIRE(dotTruePos < dotFalsePos);
}

#include "TopMu.h"
#include "TopInt.h"
#include "TopVar.h"
#include "ASTNumberExpr.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <memory>
#include <sstream>

TEST_CASE("TopMu: test TopMus are compared by their underlying t and v",
          "[TopMu]") {
  auto term = std::make_shared<TopInt>();
  ASTNumberExpr n(42);
  auto var = std::make_shared<TopVar>(&n);

  TopMu mu(var, term);
  TopMu mu2(var, term);
  REQUIRE(mu == mu2);
}

TEST_CASE("TopMu: test not equals",
          "[TopMu]") {
  auto term = std::make_shared<TopInt>();
  ASTNumberExpr n(41);
  ASTNumberExpr n2(42);
  auto var = std::make_shared<TopVar>(&n);
  auto var2 = std::make_shared<TopVar>(&n2);

  TopMu mu(var, term);
  TopMu mu2(var2, term);
  REQUIRE(mu != mu2);
}

TEST_CASE("TopMu: test comparison with a different type",
          "[TopMu]") {
  auto term = std::make_shared<TopInt>();
  ASTNumberExpr n(41);
  auto var = std::make_shared<TopVar>(&n);
  TopMu mu(var, term);

  TopInt tipInt;

  REQUIRE_FALSE(mu == tipInt);
}

TEST_CASE("TopMu: test Getters", "[TopMu]") {
  auto term = std::make_shared<TopInt>();
  ASTNumberExpr n(42);
  auto var = std::make_shared<TopVar>(&n);
  TopMu mu(var, term);

  REQUIRE(term == mu.getT());
  REQUIRE(var == mu.getV());
}

TEST_CASE("TopMu: test output stream", "[TopMu]") {
  auto term = std::make_shared<TopInt>();
  ASTNumberExpr n(42);
  auto var = std::make_shared<TopVar>(&n);
  TopMu mu(var, term);
  std::stringstream stream;
  stream << mu;

  auto actual = stream.str();
  REQUIRE_THAT(actual,
               Catch::Matchers::Matches("^μ\u27E642@\\d+:\\d+\u27E7\\.int$"));
}

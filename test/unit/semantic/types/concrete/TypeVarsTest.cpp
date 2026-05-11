#include "TypeVars.h"
#include "TipInt.h"
#include "TipVar.h"
#include "TipAlpha.h"
#include "TipRef.h"
#include "TipFunction.h"
#include "TipMu.h"
#include "ASTNumberExpr.h"

#include <catch2/catch_test_macros.hpp>
#include <memory>

// Helpers
static ASTNumberExpr nodeX(1);
static ASTNumberExpr nodeY(2);

TEST_CASE("TypeVars: nullary type has no free variables", "[TypeVars]") {
  auto t = std::make_shared<TipInt>();
  auto vars = TypeVars::collect(t.get());
  REQUIRE(vars.empty());
}

TEST_CASE("TypeVars: single TipVar is collected", "[TypeVars]") {
  auto v = std::make_shared<TipVar>(&nodeX);
  auto vars = TypeVars::collect(v.get());
  REQUIRE(vars.size() == 1);
  REQUIRE((*vars.begin())->getNode() == &nodeX);
}

TEST_CASE("TypeVars: TipAlpha is included in the set", "[TypeVars]") {
  // TipAlpha::endVisit inserts a TipAlpha — it IS counted as a type variable
  auto alpha = std::make_shared<TipAlpha>(&nodeX, std::string("0"));
  auto vars = TypeVars::collect(alpha.get());
  REQUIRE(vars.size() == 1);
}

TEST_CASE("TypeVars: TipVar inside compound type is collected", "[TypeVars]") {
  auto v = std::make_shared<TipVar>(&nodeX);
  auto ref = std::make_shared<TipRef>(v);
  auto vars = TypeVars::collect(ref.get());
  REQUIRE(vars.size() == 1);
  REQUIRE((*vars.begin())->getNode() == &nodeX);
}

TEST_CASE("TypeVars: same logical variable appearing twice is deduplicated", "[TypeVars]") {
  // After Phase 3 TipVarValueCmp fix: two TipVar wrappers for the same node
  // compare equal in the set, so only one entry is kept.
  auto v1 = std::make_shared<TipVar>(&nodeX);
  auto v2 = std::make_shared<TipVar>(&nodeX);
  auto func = std::make_shared<TipFunction>(
      std::vector<std::shared_ptr<TipType>>{v1}, v2);
  auto vars = TypeVars::collect(func.get());
  REQUIRE(vars.size() == 1);
  REQUIRE((*vars.begin())->getNode() == &nodeX);
}

TEST_CASE("TypeVars: two different variables both collected", "[TypeVars]") {
  auto vx = std::make_shared<TipVar>(&nodeX);
  auto vy = std::make_shared<TipVar>(&nodeY);
  auto func = std::make_shared<TipFunction>(
      std::vector<std::shared_ptr<TipType>>{vx}, vy);
  auto vars = TypeVars::collect(func.get());
  REQUIRE(vars.size() == 2);
}

TEST_CASE("TypeVars: TipMu bound variable is removed from set", "[TypeVars]") {
  // After Phase 3 fix: vars.erase(element->getV()) works because TipVarValueCmp
  // compares by node pointer value, so the newly-inserted TipVar(nodeX) is
  // found and removed when the TipMu binding variable (also nodeX) is erased.
  auto v = std::make_shared<TipVar>(&nodeX);
  auto body = std::make_shared<TipRef>(v);
  auto mu = std::make_shared<TipMu>(v, body);

  auto vars = TypeVars::collect(mu.get());
  // v is the only variable; since it is bound by μ, it should be removed.
  REQUIRE(vars.empty());
}

TEST_CASE("TypeVars: free variable inside TipMu body is collected", "[TypeVars]") {
  // TipMu(v, TipFunction([v, x], int)) — v is bound, x is free
  auto v  = std::make_shared<TipVar>(&nodeX);
  auto x  = std::make_shared<TipVar>(&nodeY);
  auto intType = std::make_shared<TipInt>();
  auto body = std::make_shared<TipFunction>(
      std::vector<std::shared_ptr<TipType>>{v, x}, intType);
  auto mu = std::make_shared<TipMu>(v, body);

  auto vars = TypeVars::collect(mu.get());
  // Only x should remain after v is erased.
  REQUIRE(vars.size() == 1);
  REQUIRE((*vars.begin())->getNode() == &nodeY);
}

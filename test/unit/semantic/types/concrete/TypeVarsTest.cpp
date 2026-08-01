#include "TypeVars.h"
#include "TopInt.h"
#include "TopVar.h"
#include "TopAlpha.h"
#include "TopOwningRef.h"
#include "TopFunction.h"
#include "TopMu.h"
#include "ASTNumberExpr.h"

#include <catch2/catch_test_macros.hpp>
#include <memory>

// Helpers
static ASTNumberExpr nodeX(1);
static ASTNumberExpr nodeY(2);

TEST_CASE("TypeVars: nullary type has no free variables", "[TypeVars]") {
  auto t = std::make_shared<TopInt>();
  auto vars = TypeVars::collect(t.get());
  REQUIRE(vars.empty());
}

TEST_CASE("TypeVars: single TopVar is collected", "[TypeVars]") {
  auto v = std::make_shared<TopVar>(&nodeX);
  auto vars = TypeVars::collect(v.get());
  REQUIRE(vars.size() == 1);
  REQUIRE((*vars.begin())->getNode() == &nodeX);
}

TEST_CASE("TypeVars: TopAlpha is included in the set", "[TypeVars]") {
  // TopAlpha::endVisit inserts a TopAlpha — it IS counted as a type variable
  auto alpha = std::make_shared<TopAlpha>(&nodeX, std::string("0"));
  auto vars = TypeVars::collect(alpha.get());
  REQUIRE(vars.size() == 1);
}

TEST_CASE("TypeVars: TopVar inside compound type is collected", "[TypeVars]") {
  auto v = std::make_shared<TopVar>(&nodeX);
  auto ref = std::make_shared<TopOwningRef>(v);
  auto vars = TypeVars::collect(ref.get());
  REQUIRE(vars.size() == 1);
  REQUIRE((*vars.begin())->getNode() == &nodeX);
}

TEST_CASE("TypeVars: same logical variable appearing twice is deduplicated", "[TypeVars]") {
  // After Phase 3 TopVarValueCmp fix: two TopVar wrappers for the same node
  // compare equal in the set, so only one entry is kept.
  auto v1 = std::make_shared<TopVar>(&nodeX);
  auto v2 = std::make_shared<TopVar>(&nodeX);
  auto func = std::make_shared<TopFunction>(
      std::vector<std::shared_ptr<TopType>>{v1}, v2);
  auto vars = TypeVars::collect(func.get());
  REQUIRE(vars.size() == 1);
  REQUIRE((*vars.begin())->getNode() == &nodeX);
}

TEST_CASE("TypeVars: two different variables both collected", "[TypeVars]") {
  auto vx = std::make_shared<TopVar>(&nodeX);
  auto vy = std::make_shared<TopVar>(&nodeY);
  auto func = std::make_shared<TopFunction>(
      std::vector<std::shared_ptr<TopType>>{vx}, vy);
  auto vars = TypeVars::collect(func.get());
  REQUIRE(vars.size() == 2);
}

TEST_CASE("TypeVars: TopMu bound variable is removed from set", "[TypeVars]") {
  // After Phase 3 fix: vars.erase(element->getV()) works because TopVarValueCmp
  // compares by node pointer value, so the newly-inserted TopVar(nodeX) is
  // found and removed when the TopMu binding variable (also nodeX) is erased.
  auto v = std::make_shared<TopVar>(&nodeX);
  auto body = std::make_shared<TopOwningRef>(v);
  auto mu = std::make_shared<TopMu>(v, body);

  auto vars = TypeVars::collect(mu.get());
  // v is the only variable; since it is bound by μ, it should be removed.
  REQUIRE(vars.empty());
}

TEST_CASE("TypeVars: free variable inside TopMu body is collected", "[TypeVars]") {
  // TopMu(v, TopFunction([v, x], int)) — v is bound, x is free
  auto v  = std::make_shared<TopVar>(&nodeX);
  auto x  = std::make_shared<TopVar>(&nodeY);
  auto intType = std::make_shared<TopInt>();
  auto body = std::make_shared<TopFunction>(
      std::vector<std::shared_ptr<TopType>>{v, x}, intType);
  auto mu = std::make_shared<TopMu>(v, body);

  auto vars = TypeVars::collect(mu.get());
  // Only x should remain after v is erased.
  REQUIRE(vars.size() == 1);
  REQUIRE((*vars.begin())->getNode() == &nodeY);
}

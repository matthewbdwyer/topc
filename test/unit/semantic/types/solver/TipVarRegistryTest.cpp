#include "TipVarRegistry.h"
#include "TipVar.h"
#include "TipAlpha.h"
#include "ASTNumberExpr.h"

#include <catch2/catch_test_macros.hpp>
#include <memory>

TEST_CASE("TipVarRegistry: lookup returns the registered pointer", "[TipVarRegistry]") {
  ASTNumberExpr nodeA(1);
  auto vA = std::make_shared<TipVar>(&nodeA);

  TipVarRegistry reg;
  reg.register_(vA);

  auto result = reg.lookup(vA->getFunctor());
  REQUIRE(result != nullptr);
  REQUIRE(result.get() == vA.get());
}

TEST_CASE("TipVarRegistry: multiple independent variables", "[TipVarRegistry]") {
  ASTNumberExpr nodeA(1);
  ASTNumberExpr nodeB(2);
  auto vA = std::make_shared<TipVar>(&nodeA);
  auto vB = std::make_shared<TipVar>(&nodeB);

  TipVarRegistry reg;
  reg.register_(vA);
  reg.register_(vB);

  REQUIRE(reg.lookup(vA->getFunctor()).get() == vA.get());
  REQUIRE(reg.lookup(vB->getFunctor()).get() == vB.get());
}

TEST_CASE("TipVarRegistry: unregistered key returns nullptr", "[TipVarRegistry]") {
  TipVarRegistry reg;
  REQUIRE(reg.lookup("no-such-key") == nullptr);

  ASTNumberExpr nodeA(1);
  auto vA = std::make_shared<TipVar>(&nodeA);
  // Registered once; a different (unregistered) var's key should not be found
  ASTNumberExpr nodeB(2);
  auto vB = std::make_shared<TipVar>(&nodeB);
  reg.register_(vA);
  REQUIRE(reg.lookup(vB->getFunctor()) == nullptr);
}

TEST_CASE("TipVarRegistry: registration is idempotent", "[TipVarRegistry]") {
  ASTNumberExpr nodeA(1);
  auto vA = std::make_shared<TipVar>(&nodeA);

  TipVarRegistry reg;
  reg.register_(vA);
  reg.register_(vA); // second call must not change anything

  auto result = reg.lookup(vA->getFunctor());
  REQUIRE(result.get() == vA.get());
}

TEST_CASE("TipVarRegistry: TipAlpha can be registered and looked up", "[TipVarRegistry]") {
  ASTNumberExpr nodeA(1);
  // TipAlpha extends TipVar, so it can be stored in the registry
  auto alpha = std::make_shared<TipAlpha>(&nodeA);

  TipVarRegistry reg;
  reg.register_(alpha);

  auto result = reg.lookup(alpha->getFunctor());
  REQUIRE(result != nullptr);
  REQUIRE(result.get() == alpha.get());
}

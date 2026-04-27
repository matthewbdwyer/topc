#include "TipTermClosure.h"
#include "TipVarRegistry.h"
#include "TipVar.h"
#include "TipAlpha.h"
#include "TipInt.h"
#include "TipRef.h"
#include "TipMu.h"
#include "TermUnifier.h"
#include "ASTNumberExpr.h"

#include <catch2/catch_test_macros.hpp>
#include <memory>

// ---------------------------------------------------------------------------
// Helper: build a TermUnifier::Substitution manually (avoids occurs-check
// for the cyclic-binding test and keeps tests independent of solve()).
// ---------------------------------------------------------------------------

using Subst = TermUnifier::Substitution;

TEST_CASE("TipTermClosure: unbound variable becomes TipAlpha", "[TipTermClosure]") {
  ASTNumberExpr nodeX(1);
  auto vx = std::make_shared<TipVar>(&nodeX);

  Subst sub;   // empty — vx is unbound
  TipVarRegistry reg;
  reg.register_(vx);

  TipTermClosure closure(sub, reg);
  auto result = closure.close(vx);

  // Unbound variable must produce a TipAlpha with the same node
  REQUIRE(std::dynamic_pointer_cast<TipAlpha>(result) != nullptr);
  auto alpha = std::dynamic_pointer_cast<TipAlpha>(result);
  REQUIRE(alpha->getNode() == &nodeX);
}

TEST_CASE("TipTermClosure: variable bound to TipInt becomes TipInt", "[TipTermClosure]") {
  ASTNumberExpr nodeX(1);
  auto vx = std::make_shared<TipVar>(&nodeX);
  auto tipInt = std::make_shared<TipInt>();

  Subst sub;
  sub[vx->getFunctor()] = tipInt;

  TipVarRegistry reg;
  reg.register_(vx);

  TipTermClosure closure(sub, reg);
  auto result = closure.close(vx);

  REQUIRE(std::dynamic_pointer_cast<TipInt>(result) != nullptr);
}

TEST_CASE("TipTermClosure: two-hop chain x→y→TipInt resolves to TipInt", "[TipTermClosure]") {
  ASTNumberExpr nodeX(1);
  ASTNumberExpr nodeY(2);
  auto vx = std::make_shared<TipVar>(&nodeX);
  auto vy = std::make_shared<TipVar>(&nodeY);
  auto tipInt = std::make_shared<TipInt>();

  Subst sub;
  sub[vx->getFunctor()] = vy;   // x → y
  sub[vy->getFunctor()] = tipInt; // y → int

  TipVarRegistry reg;
  reg.register_(vx);
  reg.register_(vy);

  TipTermClosure closure(sub, reg);
  auto result = closure.close(vx);

  REQUIRE(std::dynamic_pointer_cast<TipInt>(result) != nullptr);
}

TEST_CASE("TipTermClosure: TipInt passthrough (no variables)", "[TipTermClosure]") {
  Subst sub;
  TipVarRegistry reg;

  TipTermClosure closure(sub, reg);
  auto tipInt = std::make_shared<TipInt>();
  auto result = closure.close(tipInt);

  REQUIRE(std::dynamic_pointer_cast<TipInt>(result) != nullptr);
}

TEST_CASE("TipTermClosure: cyclic binding x = TipRef(x) produces TipMu", "[TipTermClosure]") {
  ASTNumberExpr nodeX(1);
  auto vx = std::make_shared<TipVar>(&nodeX);
  auto refX = std::make_shared<TipRef>(vx);

  // Manually insert the cyclic binding — bypasses TermUnifier's occurs check
  Subst sub;
  sub[vx->getFunctor()] = refX;

  TipVarRegistry reg;
  reg.register_(vx);

  TipTermClosure closure(sub, reg);
  auto result = closure.close(vx);

  // Result must be a TipMu wrapping a TipRef
  auto mu = std::dynamic_pointer_cast<TipMu>(result);
  REQUIRE(mu != nullptr);
  REQUIRE(std::dynamic_pointer_cast<TipRef>(mu->getT()) != nullptr);
}

TEST_CASE("TipTermClosure: TipRef with free variable", "[TipTermClosure]") {
  ASTNumberExpr nodeX(1);
  auto vx = std::make_shared<TipVar>(&nodeX);
  auto vxBound = std::make_shared<TipInt>();

  auto refX = std::make_shared<TipRef>(vx);

  Subst sub;
  sub[vx->getFunctor()] = vxBound;  // x bound to int

  TipVarRegistry reg;
  reg.register_(vx);

  TipTermClosure closure(sub, reg);
  auto result = closure.close(refX);

  // close(TipRef(x)) where x→int  →  TipRef(TipInt)
  auto ref = std::dynamic_pointer_cast<TipRef>(result);
  REQUIRE(ref != nullptr);
  REQUIRE(std::dynamic_pointer_cast<TipInt>(ref->getReferencedType()) != nullptr);
}

TEST_CASE("TipTermClosure: TipRef with unbound variable becomes TipRef(TipAlpha)", "[TipTermClosure]") {
  ASTNumberExpr nodeX(1);
  auto vx = std::make_shared<TipVar>(&nodeX);
  auto refX = std::make_shared<TipRef>(vx);

  Subst sub;  // empty — x is unbound
  TipVarRegistry reg;
  reg.register_(vx);

  TipTermClosure closure(sub, reg);
  auto result = closure.close(refX);

  auto ref = std::dynamic_pointer_cast<TipRef>(result);
  REQUIRE(ref != nullptr);
  REQUIRE(std::dynamic_pointer_cast<TipAlpha>(ref->getReferencedType()) != nullptr);
}

TEST_CASE("TipTermClosure: TipMu passthrough (already formed)", "[TipTermClosure]") {
  ASTNumberExpr nodeX(1);
  auto vx = std::make_shared<TipVar>(&nodeX);
  auto mu = std::make_shared<TipMu>(vx, std::make_shared<TipRef>(vx));

  Subst sub;  // no bindings
  TipVarRegistry reg;
  reg.register_(vx);

  TipTermClosure closure(sub, reg);
  auto result = closure.close(mu);

  // close(mu(x, Ref(x))) with empty substitution:
  // recurses into the body; x is unbound so becomes TipAlpha
  auto resultMu = std::dynamic_pointer_cast<TipMu>(result);
  REQUIRE(resultMu != nullptr);
  REQUIRE(std::dynamic_pointer_cast<TipRef>(resultMu->getT()) != nullptr);
}

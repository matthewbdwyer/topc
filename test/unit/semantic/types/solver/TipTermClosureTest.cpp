#include "TipTypeClosure.h"
#include "TipTermAdapter.h"
#include "TipVar.h"
#include "TipAlpha.h"
#include "TipInt.h"
#include "TipRef.h"
#include "TipMu.h"
#include "TermUnifier.h"
#include "ASTNumberExpr.h"

#include <catch2/catch_test_macros.hpp>
#include <memory>

TEST_CASE("TipTypeClosure: unbound variable becomes TipAlpha", "[TipTypeClosure]") {
  ASTNumberExpr nodeX(1);
  auto vx = std::make_shared<TipVar>(&nodeX);

  // Empty unifier — vx is unbound
  TermUnifier unifier;
  unifier.solve();

  TipTypeClosure closure(unifier);
  auto result = closure.close(vx);

  // Unbound variable must produce a TipAlpha with the same node
  REQUIRE(std::dynamic_pointer_cast<TipAlpha>(result) != nullptr);
  auto alpha = std::dynamic_pointer_cast<TipAlpha>(result);
  REQUIRE(alpha->getNode() == &nodeX);
}

TEST_CASE("TipTypeClosure: variable bound to TipInt becomes TipInt", "[TipTypeClosure]") {
  ASTNumberExpr nodeX(1);
  auto vx = std::make_shared<TipVar>(&nodeX);
  auto tipInt = std::make_shared<TipInt>();

  TermUnifier unifier;
  unifier.addConstraint(TipTermAdapter::wrap(vx), TipTermAdapter::wrap(tipInt));
  unifier.solve();

  TipTypeClosure closure(unifier);
  auto result = closure.close(vx);

  REQUIRE(std::dynamic_pointer_cast<TipInt>(result) != nullptr);
}

TEST_CASE("TipTypeClosure: two-hop chain x→y→TipInt resolves to TipInt", "[TipTypeClosure]") {
  ASTNumberExpr nodeX(1);
  ASTNumberExpr nodeY(2);
  auto vx = std::make_shared<TipVar>(&nodeX);
  auto vy = std::make_shared<TipVar>(&nodeY);
  auto tipInt = std::make_shared<TipInt>();

  TermUnifier unifier;
  unifier.addConstraint(TipTermAdapter::wrap(vx), TipTermAdapter::wrap(vy));    // x → y
  unifier.addConstraint(TipTermAdapter::wrap(vy), TipTermAdapter::wrap(tipInt)); // y → int
  unifier.solve();

  TipTypeClosure closure(unifier);
  auto result = closure.close(vx);

  REQUIRE(std::dynamic_pointer_cast<TipInt>(result) != nullptr);
}

TEST_CASE("TipTypeClosure: TipInt passthrough (no variables)", "[TipTypeClosure]") {
  TermUnifier unifier;
  unifier.solve();

  TipTypeClosure closure(unifier);
  auto tipInt = std::make_shared<TipInt>();
  auto result = closure.close(tipInt);

  REQUIRE(std::dynamic_pointer_cast<TipInt>(result) != nullptr);
}

TEST_CASE("TipTypeClosure: cyclic binding x = TipRef(x) produces TipMu", "[TipTypeClosure]") {
  ASTNumberExpr nodeX(1);
  auto vx = std::make_shared<TipVar>(&nodeX);
  auto refX = std::make_shared<TipRef>(vx);

  // No occurs check — cyclic constraint is accepted and produces TipMu via close()
  TermUnifier unifier;
  unifier.addConstraint(TipTermAdapter::wrap(vx), TipTermAdapter::wrap(refX));
  unifier.solve();

  TipTypeClosure closure(unifier);
  auto result = closure.close(vx);

  // Result must be a TipMu wrapping a TipRef
  auto mu = std::dynamic_pointer_cast<TipMu>(result);
  REQUIRE(mu != nullptr);
  REQUIRE(std::dynamic_pointer_cast<TipRef>(mu->getT()) != nullptr);
}

TEST_CASE("TipTypeClosure: TipRef with free variable", "[TipTypeClosure]") {
  ASTNumberExpr nodeX(1);
  auto vx = std::make_shared<TipVar>(&nodeX);
  auto tipInt = std::make_shared<TipInt>();
  auto refX = std::make_shared<TipRef>(vx);

  TermUnifier unifier;
  unifier.addConstraint(TipTermAdapter::wrap(vx), TipTermAdapter::wrap(tipInt)); // x bound to int
  unifier.solve();

  TipTypeClosure closure(unifier);
  auto result = closure.close(refX);

  // close(TipRef(x)) where x→int  →  TipRef(TipInt)
  auto ref = std::dynamic_pointer_cast<TipRef>(result);
  REQUIRE(ref != nullptr);
  REQUIRE(std::dynamic_pointer_cast<TipInt>(ref->getReferencedType()) != nullptr);
}

TEST_CASE("TipTypeClosure: TipRef with unbound variable becomes TipRef(TipAlpha)", "[TipTypeClosure]") {
  ASTNumberExpr nodeX(1);
  auto vx = std::make_shared<TipVar>(&nodeX);
  auto refX = std::make_shared<TipRef>(vx);

  TermUnifier unifier; // empty — x is unbound
  unifier.solve();

  TipTypeClosure closure(unifier);
  auto result = closure.close(refX);

  auto ref = std::dynamic_pointer_cast<TipRef>(result);
  REQUIRE(ref != nullptr);
  REQUIRE(std::dynamic_pointer_cast<TipAlpha>(ref->getReferencedType()) != nullptr);
}

TEST_CASE("TipTypeClosure: TipMu passthrough (already formed)", "[TipTypeClosure]") {
  ASTNumberExpr nodeX(1);
  auto vx = std::make_shared<TipVar>(&nodeX);
  auto mu = std::make_shared<TipMu>(vx, std::make_shared<TipRef>(vx));

  TermUnifier unifier; // no bindings
  unifier.solve();

  TipTypeClosure closure(unifier);
  auto result = closure.close(mu);

  // close(mu(x, Ref(x))) with empty substitution:
  // recurses into the body; x is unbound so becomes TipAlpha
  auto resultMu = std::dynamic_pointer_cast<TipMu>(result);
  REQUIRE(resultMu != nullptr);
  REQUIRE(std::dynamic_pointer_cast<TipRef>(resultMu->getT()) != nullptr);
}


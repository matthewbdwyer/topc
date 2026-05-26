#include "TopTypeClosure.h"
#include "TopTermAdapter.h"
#include "TopVar.h"
#include "TopAlpha.h"
#include "TopInt.h"
#include "TopRef.h"
#include "TopMu.h"
#include "TermUnifier.h"
#include "ASTNumberExpr.h"

#include <catch2/catch_test_macros.hpp>
#include <memory>

TEST_CASE("TopTypeClosure: unbound variable becomes TopAlpha", "[TopTypeClosure]") {
  ASTNumberExpr nodeX(1);
  auto vx = std::make_shared<TopVar>(&nodeX);

  // Empty unifier — vx is unbound
  TermUnifier unifier;
  unifier.solve();

  TopTypeClosure closure(unifier);
  auto result = closure.close(vx);

  // Unbound variable must produce a TopAlpha with the same node
  REQUIRE(std::dynamic_pointer_cast<TopAlpha>(result) != nullptr);
  auto alpha = std::dynamic_pointer_cast<TopAlpha>(result);
  REQUIRE(alpha->getNode() == &nodeX);
}

TEST_CASE("TopTypeClosure: variable bound to TopInt becomes TopInt", "[TopTypeClosure]") {
  ASTNumberExpr nodeX(1);
  auto vx = std::make_shared<TopVar>(&nodeX);
  auto tipInt = std::make_shared<TopInt>();

  TermUnifier unifier;
  unifier.addConstraint(TopTermAdapter::wrap(vx), TopTermAdapter::wrap(tipInt));
  unifier.solve();

  TopTypeClosure closure(unifier);
  auto result = closure.close(vx);

  REQUIRE(std::dynamic_pointer_cast<TopInt>(result) != nullptr);
}

TEST_CASE("TopTypeClosure: two-hop chain x→y→TopInt resolves to TopInt", "[TopTypeClosure]") {
  ASTNumberExpr nodeX(1);
  ASTNumberExpr nodeY(2);
  auto vx = std::make_shared<TopVar>(&nodeX);
  auto vy = std::make_shared<TopVar>(&nodeY);
  auto tipInt = std::make_shared<TopInt>();

  TermUnifier unifier;
  unifier.addConstraint(TopTermAdapter::wrap(vx), TopTermAdapter::wrap(vy));    // x → y
  unifier.addConstraint(TopTermAdapter::wrap(vy), TopTermAdapter::wrap(tipInt)); // y → int
  unifier.solve();

  TopTypeClosure closure(unifier);
  auto result = closure.close(vx);

  REQUIRE(std::dynamic_pointer_cast<TopInt>(result) != nullptr);
}

TEST_CASE("TopTypeClosure: TopInt passthrough (no variables)", "[TopTypeClosure]") {
  TermUnifier unifier;
  unifier.solve();

  TopTypeClosure closure(unifier);
  auto tipInt = std::make_shared<TopInt>();
  auto result = closure.close(tipInt);

  REQUIRE(std::dynamic_pointer_cast<TopInt>(result) != nullptr);
}

TEST_CASE("TopTypeClosure: cyclic binding x = TopRef(x) produces TopMu", "[TopTypeClosure]") {
  ASTNumberExpr nodeX(1);
  auto vx = std::make_shared<TopVar>(&nodeX);
  auto refX = std::make_shared<TopRef>(vx);

  // No occurs check — cyclic constraint is accepted and produces TopMu via close()
  TermUnifier unifier;
  unifier.addConstraint(TopTermAdapter::wrap(vx), TopTermAdapter::wrap(refX));
  unifier.solve();

  TopTypeClosure closure(unifier);
  auto result = closure.close(vx);

  // Result must be a TopMu wrapping a TopRef
  auto mu = std::dynamic_pointer_cast<TopMu>(result);
  REQUIRE(mu != nullptr);
  REQUIRE(std::dynamic_pointer_cast<TopRef>(mu->getT()) != nullptr);
}

TEST_CASE("TopTypeClosure: TopRef with free variable", "[TopTypeClosure]") {
  ASTNumberExpr nodeX(1);
  auto vx = std::make_shared<TopVar>(&nodeX);
  auto tipInt = std::make_shared<TopInt>();
  auto refX = std::make_shared<TopRef>(vx);

  TermUnifier unifier;
  unifier.addConstraint(TopTermAdapter::wrap(vx), TopTermAdapter::wrap(tipInt)); // x bound to int
  unifier.solve();

  TopTypeClosure closure(unifier);
  auto result = closure.close(refX);

  // close(TopRef(x)) where x→int  →  TopRef(TopInt)
  auto ref = std::dynamic_pointer_cast<TopRef>(result);
  REQUIRE(ref != nullptr);
  REQUIRE(std::dynamic_pointer_cast<TopInt>(ref->getReferencedType()) != nullptr);
}

TEST_CASE("TopTypeClosure: TopRef with unbound variable becomes TopRef(TopAlpha)", "[TopTypeClosure]") {
  ASTNumberExpr nodeX(1);
  auto vx = std::make_shared<TopVar>(&nodeX);
  auto refX = std::make_shared<TopRef>(vx);

  TermUnifier unifier; // empty — x is unbound
  unifier.solve();

  TopTypeClosure closure(unifier);
  auto result = closure.close(refX);

  auto ref = std::dynamic_pointer_cast<TopRef>(result);
  REQUIRE(ref != nullptr);
  REQUIRE(std::dynamic_pointer_cast<TopAlpha>(ref->getReferencedType()) != nullptr);
}

TEST_CASE("TopTypeClosure: TopMu passthrough (already formed)", "[TopTypeClosure]") {
  ASTNumberExpr nodeX(1);
  auto vx = std::make_shared<TopVar>(&nodeX);
  auto mu = std::make_shared<TopMu>(vx, std::make_shared<TopRef>(vx));

  TermUnifier unifier; // no bindings
  unifier.solve();

  TopTypeClosure closure(unifier);
  auto result = closure.close(mu);

  // close(mu(x, Ref(x))) with empty substitution:
  // recurses into the body; x is unbound so becomes TopAlpha
  auto resultMu = std::dynamic_pointer_cast<TopMu>(result);
  REQUIRE(resultMu != nullptr);
  REQUIRE(std::dynamic_pointer_cast<TopRef>(resultMu->getT()) != nullptr);
}


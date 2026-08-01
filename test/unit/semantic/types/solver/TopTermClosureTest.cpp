#include "TopTypeClosure.h"
#include "TopTermAdapter.h"
#include "TopVar.h"
#include "TopAlpha.h"
#include "TopBorrowRef.h"
#include "TopInt.h"
#include "TopModeVar.h"
#include "TopOwningRef.h"
#include "TopMu.h"
#include "ReferenceMode.h"
#include "ReferenceType.h"
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

TEST_CASE("TopTypeClosure: cyclic binding x = TopOwningRef(x) produces TopMu", "[TopTypeClosure]") {
  ASTNumberExpr nodeX(1);
  auto vx = std::make_shared<TopVar>(&nodeX);
  auto refX = std::make_shared<TopOwningRef>(vx);

  // No occurs check — cyclic constraint is accepted and produces TopMu via close()
  TermUnifier unifier;
  unifier.addConstraint(TopTermAdapter::wrap(vx), TopTermAdapter::wrap(refX));
  unifier.solve();

  TopTypeClosure closure(unifier);
  auto result = closure.close(vx);

  // Result must be a TopMu wrapping a TopOwningRef
  auto mu = std::dynamic_pointer_cast<TopMu>(result);
  REQUIRE(mu != nullptr);
  REQUIRE(std::dynamic_pointer_cast<TopOwningRef>(mu->getT()) != nullptr);
}

TEST_CASE("TopTypeClosure: TopOwningRef with free variable", "[TopTypeClosure]") {
  ASTNumberExpr nodeX(1);
  auto vx = std::make_shared<TopVar>(&nodeX);
  auto tipInt = std::make_shared<TopInt>();
  auto refX = std::make_shared<TopOwningRef>(vx);

  TermUnifier unifier;
  unifier.addConstraint(TopTermAdapter::wrap(vx), TopTermAdapter::wrap(tipInt)); // x bound to int
  unifier.solve();

  TopTypeClosure closure(unifier);
  auto result = closure.close(refX);

  // close(TopOwningRef(x)) where x→int  →  TopOwningRef(TopInt)
  auto ref = std::dynamic_pointer_cast<TopOwningRef>(result);
  REQUIRE(ref != nullptr);
  REQUIRE(std::dynamic_pointer_cast<TopInt>(ref->getReferencedType()) != nullptr);
}

TEST_CASE("TopTypeClosure: TopOwningRef with unbound variable becomes TopOwningRef(TopAlpha)", "[TopTypeClosure]") {
  ASTNumberExpr nodeX(1);
  auto vx = std::make_shared<TopVar>(&nodeX);
  auto refX = std::make_shared<TopOwningRef>(vx);

  TermUnifier unifier; // empty — x is unbound
  unifier.solve();

  TopTypeClosure closure(unifier);
  auto result = closure.close(refX);

  auto ref = std::dynamic_pointer_cast<TopOwningRef>(result);
  REQUIRE(ref != nullptr);
  REQUIRE(std::dynamic_pointer_cast<TopAlpha>(ref->getReferencedType()) != nullptr);
}

TEST_CASE("TopTypeClosure: TopMu passthrough (already formed)", "[TopTypeClosure]") {
  ASTNumberExpr nodeX(1);
  auto vx = std::make_shared<TopVar>(&nodeX);
  auto mu = std::make_shared<TopMu>(vx, std::make_shared<TopOwningRef>(vx));

  TermUnifier unifier; // no bindings
  unifier.solve();

  TopTypeClosure closure(unifier);
  auto result = closure.close(mu);

  // close(mu(x, Ref(x))) with empty substitution:
  // recurses into the body; x is unbound so becomes TopAlpha
  auto resultMu = std::dynamic_pointer_cast<TopMu>(result);
  REQUIRE(resultMu != nullptr);
  REQUIRE(std::dynamic_pointer_cast<TopOwningRef>(resultMu->getT()) != nullptr);
}

  TEST_CASE("TopTypeClosure: resolved Own mode canonicalizes ReferenceType",
        "[TopTypeClosure][ReferenceMode]") {
    auto mode = std::make_shared<TopModeVar>();
    auto reference =
      std::make_shared<ReferenceType>(mode, std::make_shared<TopInt>());
    auto own =
      std::make_shared<ReferenceMode>(ReferenceMode::Mode::Own);

    TermUnifier unifier;
    unifier.addConstraint(TopTermAdapter::wrap(mode),
              TopTermAdapter::wrap(own));
    unifier.solve();

    auto result = TopTypeClosure(unifier).close(reference);
    auto owning = std::dynamic_pointer_cast<TopOwningRef>(result);
    REQUIRE(owning != nullptr);
    REQUIRE(std::dynamic_pointer_cast<TopInt>(owning->getReferencedType()) !=
        nullptr);
  }

  TEST_CASE("TopTypeClosure: resolved Borrow mode canonicalizes ReferenceType",
        "[TopTypeClosure][ReferenceMode]") {
    auto mode = std::make_shared<TopModeVar>();
    auto reference =
      std::make_shared<ReferenceType>(mode, std::make_shared<TopInt>());
    auto borrow =
      std::make_shared<ReferenceMode>(ReferenceMode::Mode::Borrow);

    TermUnifier unifier;
    unifier.addConstraint(TopTermAdapter::wrap(mode),
              TopTermAdapter::wrap(borrow));
    unifier.solve();

    auto result = TopTypeClosure(unifier).close(reference);
    auto borrowed = std::dynamic_pointer_cast<TopBorrowRef>(result);
    REQUIRE(borrowed != nullptr);
    REQUIRE(std::dynamic_pointer_cast<TopInt>(borrowed->getReferencedType()) !=
        nullptr);
  }

  TEST_CASE("TopTypeClosure: unresolved mode remains polymorphic",
        "[TopTypeClosure][ReferenceMode]") {
    auto mode = std::make_shared<TopModeVar>();
    auto reference =
      std::make_shared<ReferenceType>(mode, std::make_shared<TopInt>());

    TermUnifier unifier;
    unifier.solve();

    auto result = TopTypeClosure(unifier).close(reference);
    auto generic = std::dynamic_pointer_cast<ReferenceType>(result);
    REQUIRE(generic != nullptr);
    auto resultMode = std::dynamic_pointer_cast<TopModeVar>(generic->getMode());
    REQUIRE(resultMode != nullptr);
    REQUIRE(resultMode->getId() == mode->getId());
  }


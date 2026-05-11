#include "Copier.h"
#include "FreshAlphaCopier.h"
#include "TipInt.h"
#include "TipVar.h"
#include "TipAlpha.h"
#include "TipRef.h"
#include "ASTNumberExpr.h"

#include <catch2/catch_test_macros.hpp>
#include <memory>

// Helpers
static ASTNumberExpr nodeA(10);
static ASTNumberExpr nodeB(20);

// ==================== Copier ====================

TEST_CASE("Copier: copy nullary type produces same functor, different pointer", "[Copier]") {
  auto orig = std::make_shared<TipInt>();
  auto copy = Copier::copy(orig);
  REQUIRE(copy->getFunctor() == "int");
  REQUIRE(copy.get() != orig.get());
}

TEST_CASE("Copier: copy compound type — inner nodes also fresh", "[Copier]") {
  auto inner = std::make_shared<TipInt>();
  auto orig = std::make_shared<TipRef>(inner);
  auto copy = Copier::copy(orig);

  REQUIRE(copy->getFunctor() == "ptr");
  REQUIRE(copy.get() != orig.get());

  auto origRef = std::dynamic_pointer_cast<TipRef>(orig);
  auto copyRef = std::dynamic_pointer_cast<TipRef>(copy);
  REQUIRE(copyRef != nullptr);
  REQUIRE(copyRef->getReferencedType().get() != origRef->getReferencedType().get());
}

TEST_CASE("Copier: copy TipVar — node pointer preserved, wrapper is new", "[Copier]") {
  auto orig = std::make_shared<TipVar>(&nodeA);
  auto copy = Copier::copy(orig);

  REQUIRE(copy.get() != orig.get());
  auto copyVar = std::dynamic_pointer_cast<TipVar>(copy);
  REQUIRE(copyVar != nullptr);
  REQUIRE(copyVar->getNode() == &nodeA);
}

TEST_CASE("Copier: copy TipAlpha — node and name preserved, wrapper is new", "[Copier]") {
  auto orig = std::make_shared<TipAlpha>(&nodeA, std::string("β"));
  auto copy = Copier::copy(orig);

  REQUIRE(copy.get() != orig.get());
  auto copyAlpha = std::dynamic_pointer_cast<TipAlpha>(copy);
  REQUIRE(copyAlpha != nullptr);
  REQUIRE(copyAlpha->getNode() == orig->getNode());
  REQUIRE(copyAlpha->getName() == orig->getName());
}

// ==================== FreshAlphaCopier ====================

TEST_CASE("FreshAlphaCopier: alpha context is replaced", "[FreshAlphaCopier]") {
  auto orig = std::make_shared<TipAlpha>(&nodeA, std::string("β"));
  REQUIRE(orig->getContext() == nullptr);

  auto copy = FreshAlphaCopier::copy(orig.get(), &nodeB);
  auto copyAlpha = std::dynamic_pointer_cast<TipAlpha>(copy);
  REQUIRE(copyAlpha != nullptr);
  // Original node preserved
  REQUIRE(copyAlpha->getNode() == &nodeA);
  // Name preserved
  REQUIRE(copyAlpha->getName() == orig->getName());
  // Context is now nodeB
  REQUIRE(copyAlpha->getContext() == &nodeB);
  // Wrapper is new
  REQUIRE(copy.get() != orig.get());
}

TEST_CASE("FreshAlphaCopier: type with no alphas is returned structurally equal", "[FreshAlphaCopier]") {
  auto orig = std::make_shared<TipInt>();
  auto copy = FreshAlphaCopier::copy(orig.get(), &nodeB);
  REQUIRE(copy->getFunctor() == "int");
  REQUIRE(copy.get() != orig.get());
}

TEST_CASE("FreshAlphaCopier: two copies with different contexts produce non-equal alphas", "[FreshAlphaCopier]") {
  auto orig = std::make_shared<TipAlpha>(&nodeA, std::string("γ"));

  auto copy1 = FreshAlphaCopier::copy(orig.get(), &nodeA);
  auto copy2 = FreshAlphaCopier::copy(orig.get(), &nodeB);

  auto alpha1 = std::dynamic_pointer_cast<TipAlpha>(copy1);
  auto alpha2 = std::dynamic_pointer_cast<TipAlpha>(copy2);
  REQUIRE(alpha1 != nullptr);
  REQUIRE(alpha2 != nullptr);
  // Different context → TipAlpha::operator== returns false
  REQUIRE(!(*alpha1 == *alpha2));
}

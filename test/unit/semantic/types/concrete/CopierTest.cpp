#include "Copier.h"
#include "FreshAlphaCopier.h"
#include "TopInt.h"
#include "TopVar.h"
#include "TopAlpha.h"
#include "TopOwningRef.h"
#include "ASTNumberExpr.h"

#include <catch2/catch_test_macros.hpp>
#include <memory>

// Helpers
static ASTNumberExpr nodeA(10);
static ASTNumberExpr nodeB(20);

// ==================== Copier ====================

TEST_CASE("Copier: copy nullary type produces same functor, different pointer", "[Copier]") {
  auto orig = std::make_shared<TopInt>();
  auto copy = Copier::copy(orig);
  REQUIRE(copy->getFunctor() == "int");
  REQUIRE(copy.get() != orig.get());
}

TEST_CASE("Copier: copy compound type — inner nodes also fresh", "[Copier]") {
  auto inner = std::make_shared<TopInt>();
  auto orig = std::make_shared<TopOwningRef>(inner);
  auto copy = Copier::copy(orig);

  REQUIRE(copy->getFunctor() == "ref");
  REQUIRE(copy.get() != orig.get());

  auto origRef = std::dynamic_pointer_cast<TopOwningRef>(orig);
  auto copyRef = std::dynamic_pointer_cast<TopOwningRef>(copy);
  REQUIRE(copyRef != nullptr);
  REQUIRE(copyRef->getReferencedType().get() != origRef->getReferencedType().get());
}

TEST_CASE("Copier: copy TopVar — node pointer preserved, wrapper is new", "[Copier]") {
  auto orig = std::make_shared<TopVar>(&nodeA);
  auto copy = Copier::copy(orig);

  REQUIRE(copy.get() != orig.get());
  auto copyVar = std::dynamic_pointer_cast<TopVar>(copy);
  REQUIRE(copyVar != nullptr);
  REQUIRE(copyVar->getNode() == &nodeA);
}

TEST_CASE("Copier: copy TopAlpha — node and name preserved, wrapper is new", "[Copier]") {
  auto orig = std::make_shared<TopAlpha>(&nodeA, std::string("β"));
  auto copy = Copier::copy(orig);

  REQUIRE(copy.get() != orig.get());
  auto copyAlpha = std::dynamic_pointer_cast<TopAlpha>(copy);
  REQUIRE(copyAlpha != nullptr);
  REQUIRE(copyAlpha->getNode() == orig->getNode());
  REQUIRE(copyAlpha->getName() == orig->getName());
}

// ==================== FreshAlphaCopier ====================

TEST_CASE("FreshAlphaCopier: alpha context is replaced", "[FreshAlphaCopier]") {
  auto orig = std::make_shared<TopAlpha>(&nodeA, std::string("β"));
  REQUIRE(orig->getContext() == nullptr);

  auto copy = FreshAlphaCopier::copy(orig.get(), &nodeB);
  auto copyAlpha = std::dynamic_pointer_cast<TopAlpha>(copy);
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
  auto orig = std::make_shared<TopInt>();
  auto copy = FreshAlphaCopier::copy(orig.get(), &nodeB);
  REQUIRE(copy->getFunctor() == "int");
  REQUIRE(copy.get() != orig.get());
}

TEST_CASE("FreshAlphaCopier: two copies with different contexts produce non-equal alphas", "[FreshAlphaCopier]") {
  auto orig = std::make_shared<TopAlpha>(&nodeA, std::string("γ"));

  auto copy1 = FreshAlphaCopier::copy(orig.get(), &nodeA);
  auto copy2 = FreshAlphaCopier::copy(orig.get(), &nodeB);

  auto alpha1 = std::dynamic_pointer_cast<TopAlpha>(copy1);
  auto alpha2 = std::dynamic_pointer_cast<TopAlpha>(copy2);
  REQUIRE(alpha1 != nullptr);
  REQUIRE(alpha2 != nullptr);
  // Different context → TopAlpha::operator== returns false
  REQUIRE(!(*alpha1 == *alpha2));
}

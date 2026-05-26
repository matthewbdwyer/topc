#include "TopInt.h"
#include "TopVar.h"
#include "TopAlpha.h"
#include "TopRef.h"
#include "TopFunction.h"
#include "TopMu.h"
#include "InternalError.h"
#include "ASTNumberExpr.h"
#include "ASTVariableExpr.h"

#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <string>

// ==================== TopInt Term Interface ====================

TEST_CASE("TopInt: Term interface - isVariable", "[TopType][Term]") {
  auto t = std::make_shared<TopInt>();
  REQUIRE_FALSE(t->isVariable());
}

TEST_CASE("TopInt: Term interface - getFunctor", "[TopType][Term]") {
  auto t = std::make_shared<TopInt>();
  REQUIRE(t->getFunctor() == "int");
}

TEST_CASE("TopInt: Term interface - arity", "[TopType][Term]") {
  auto t = std::make_shared<TopInt>();
  REQUIRE(t->arity() == 0);
}

TEST_CASE("TopInt: Term interface - getSubterms", "[TopType][Term]") {
  auto t = std::make_shared<TopInt>();
  REQUIRE(t->getChildTypes().empty());
}

TEST_CASE("TopInt: Term interface - withSubterms empty", "[TopType][Term]") {
  auto t = std::make_shared<TopInt>();
  auto t2 = t->withChildTypes({});
  REQUIRE(t2->getFunctor() == "int");
  REQUIRE(*t == *t2);
}

TEST_CASE("TopInt: Term interface - withSubterms throws on non-empty", "[TopType][Term]") {
  auto t = std::make_shared<TopInt>();
  REQUIRE_THROWS_AS(t->withChildTypes({std::make_shared<TopInt>()}), std::invalid_argument);
}

TEST_CASE("TopInt: Term interface - toString", "[TopType][Term]") {
  auto t = std::make_shared<TopInt>();
  REQUIRE(t->toString() == "int");
}

TEST_CASE("TopInt: Term interface - equals", "[TopType][Term]") {
  auto t1 = std::make_shared<TopInt>();
  auto t2 = std::make_shared<TopInt>();
  REQUIRE(*t1 == *t2);
  REQUIRE(*t2 == *t1);
}

// ==================== TopVar Term Interface ====================

TEST_CASE("TopVar: Term interface - isVariable", "[TopType][Term]") {
  auto node = std::make_unique<ASTNumberExpr>(42);
  auto t = std::make_shared<TopVar>(node.get());
  REQUIRE(t->isVariable());
}

TEST_CASE("TopVar: Term interface - getFunctor contains address", "[TopType][Term]") {
  auto node = std::make_unique<ASTNumberExpr>(42);
  auto t = std::make_shared<TopVar>(node.get());
  std::string functor = t->getFunctor();
  REQUIRE(functor.find("var@") == 0);
}

TEST_CASE("TopVar: Term interface - arity", "[TopType][Term]") {
  auto node = std::make_unique<ASTNumberExpr>(42);
  auto t = std::make_shared<TopVar>(node.get());
  REQUIRE(t->arity() == 0);
}

TEST_CASE("TopVar: Term interface - getSubterms", "[TopType][Term]") {
  auto node = std::make_unique<ASTNumberExpr>(42);
  auto t = std::make_shared<TopVar>(node.get());
  REQUIRE(t->getChildTypes().empty());
}

TEST_CASE("TopVar: Term interface - withSubterms preserves node", "[TopType][Term]") {
  auto node = std::make_unique<ASTNumberExpr>(42);
  auto t = std::make_shared<TopVar>(node.get());
  auto t2 = std::dynamic_pointer_cast<TopVar>(t->withChildTypes({}));
  REQUIRE(t2 != nullptr);
  REQUIRE(t2->getNode() == node.get());
}

TEST_CASE("TopVar: Term interface - different nodes have different functors", "[TopType][Term]") {
  auto node1 = std::make_unique<ASTNumberExpr>(1);
  auto node2 = std::make_unique<ASTNumberExpr>(2);
  auto t1 = std::make_shared<TopVar>(node1.get());
  auto t2 = std::make_shared<TopVar>(node2.get());
  REQUIRE(t1->getFunctor() != t2->getFunctor());
}

// ==================== TopAlpha Term Interface ====================

TEST_CASE("TopAlpha: Term interface - isVariable is FALSE", "[TopType][Term]") {
  auto node = std::make_unique<ASTNumberExpr>(42);
  auto alpha = std::make_shared<TopAlpha>(node.get(), "0");
  REQUIRE_FALSE(alpha->isVariable());  // Critical: TopAlpha is NOT a unification variable
}

TEST_CASE("TopAlpha: Term interface - getFunctor includes name", "[TopType][Term]") {
  auto node = std::make_unique<ASTNumberExpr>(42);
  auto alpha = std::make_shared<TopAlpha>(node.get(), "0");
  REQUIRE(alpha->getFunctor() == "α0");
}

TEST_CASE("TopAlpha: Term interface - arity", "[TopType][Term]") {
  auto node = std::make_unique<ASTNumberExpr>(42);
  auto alpha = std::make_shared<TopAlpha>(node.get(), "0");
  REQUIRE(alpha->arity() == 0);
}

TEST_CASE("TopAlpha: Term interface - getSubterms", "[TopType][Term]") {
  auto node = std::make_unique<ASTNumberExpr>(42);
  auto alpha = std::make_shared<TopAlpha>(node.get(), "0");
  REQUIRE(alpha->getChildTypes().empty());
}

// ==================== TopRef Term Interface ====================

TEST_CASE("TopRef: Term interface - isVariable", "[TopType][Term]") {
  auto inner = std::make_shared<TopInt>();
  auto t = std::make_shared<TopRef>(inner);
  REQUIRE_FALSE(t->isVariable());
}

TEST_CASE("TopRef: Term interface - getFunctor", "[TopType][Term]") {
  auto inner = std::make_shared<TopInt>();
  auto t = std::make_shared<TopRef>(inner);
  REQUIRE(t->getFunctor() == "ptr");
}

TEST_CASE("TopRef: Term interface - arity", "[TopType][Term]") {
  auto inner = std::make_shared<TopInt>();
  auto t = std::make_shared<TopRef>(inner);
  REQUIRE(t->arity() == 1);
}

TEST_CASE("TopRef: Term interface - getSubterms", "[TopType][Term]") {
  auto inner = std::make_shared<TopInt>();
  auto t = std::make_shared<TopRef>(inner);
  auto subs = t->getChildTypes();
  REQUIRE(subs.size() == 1);
  REQUIRE(subs[0]->getFunctor() == "int");
}

TEST_CASE("TopRef: Term interface - withSubterms", "[TopType][Term]") {
  auto inner = std::make_shared<TopInt>();
  auto t = std::make_shared<TopRef>(inner);
  auto newInner = std::make_shared<TopRef>(inner);
  auto t2 = t->withChildTypes({newInner});
  REQUIRE(t2->getFunctor() == "ptr");
  auto subs = t2->getChildTypes();
  REQUIRE(subs.size() == 1);
  REQUIRE(subs[0]->getFunctor() == "ptr");
}

TEST_CASE("TopRef: Term interface - withSubterms wrong count throws", "[TopType][Term]") {
  auto inner = std::make_shared<TopInt>();
  auto t = std::make_shared<TopRef>(inner);
  REQUIRE_THROWS_AS(t->withChildTypes({}), std::invalid_argument);
  REQUIRE_THROWS_AS(t->withChildTypes({inner, inner}), std::invalid_argument);
}

// ==================== TopFunction Term Interface ====================

TEST_CASE("TopFunction: Term interface - isVariable", "[TopType][Term]") {
  auto intType = std::make_shared<TopInt>();
  auto func = std::make_shared<TopFunction>(
      std::vector<std::shared_ptr<TopType>>{intType}, intType);
  REQUIRE_FALSE(func->isVariable());
}

TEST_CASE("TopFunction: Term interface - getFunctor", "[TopType][Term]") {
  auto intType = std::make_shared<TopInt>();
  auto func = std::make_shared<TopFunction>(
      std::vector<std::shared_ptr<TopType>>{intType}, intType);
  REQUIRE(func->getFunctor() == "->");
}

TEST_CASE("TopFunction: Term interface - arity with one param", "[TopType][Term]") {
  auto intType = std::make_shared<TopInt>();
  auto func = std::make_shared<TopFunction>(
      std::vector<std::shared_ptr<TopType>>{intType}, intType);
  REQUIRE(func->arity() == 2);  // 1 param + 1 return
}

TEST_CASE("TopFunction: Term interface - arity with multiple params", "[TopType][Term]") {
  auto intType = std::make_shared<TopInt>();
  auto func = std::make_shared<TopFunction>(
      std::vector<std::shared_ptr<TopType>>{intType, intType, intType}, intType);
  REQUIRE(func->arity() == 4);  // 3 params + 1 return
}

TEST_CASE("TopFunction: Term interface - arity with no params", "[TopType][Term]") {
  auto intType = std::make_shared<TopInt>();
  auto func = std::make_shared<TopFunction>(
      std::vector<std::shared_ptr<TopType>>{}, intType);
  REQUIRE(func->arity() == 1);  // 0 params + 1 return
}

TEST_CASE("TopFunction: Term interface - getSubterms", "[TopType][Term]") {
  auto intType = std::make_shared<TopInt>();
  auto ptrType = std::make_shared<TopRef>(intType);
  auto func = std::make_shared<TopFunction>(
      std::vector<std::shared_ptr<TopType>>{intType, ptrType}, intType);
  auto subs = func->getChildTypes();
  REQUIRE(subs.size() == 3);
  REQUIRE(subs[0]->getFunctor() == "int");
  REQUIRE(subs[1]->getFunctor() == "ptr");
  REQUIRE(subs[2]->getFunctor() == "int");
}

TEST_CASE("TopFunction: Term interface - withSubterms", "[TopType][Term]") {
  auto intType = std::make_shared<TopInt>();
  auto func = std::make_shared<TopFunction>(
      std::vector<std::shared_ptr<TopType>>{intType}, intType);
  auto ptrType = std::make_shared<TopRef>(intType);
  auto func2 = func->withChildTypes({ptrType, ptrType});
  auto subs = func2->getChildTypes();
  REQUIRE(subs.size() == 2);
  REQUIRE(subs[0]->getFunctor() == "ptr");
  REQUIRE(subs[1]->getFunctor() == "ptr");
}

// ==================== TopMu Term Interface ====================

TEST_CASE("TopMu: Term interface - isVariable", "[TopType][Term]") {
  auto node = std::make_unique<ASTNumberExpr>(42);
  auto alpha = std::make_shared<TopAlpha>(node.get(), "0");
  auto body = std::make_shared<TopRef>(alpha);
  auto mu = std::make_shared<TopMu>(alpha, body);
  REQUIRE_FALSE(mu->isVariable());
}

TEST_CASE("TopMu: Term interface - getFunctor", "[TopType][Term]") {
  auto node = std::make_unique<ASTNumberExpr>(42);
  auto alpha = std::make_shared<TopAlpha>(node.get(), "0");
  auto body = std::make_shared<TopRef>(alpha);
  auto mu = std::make_shared<TopMu>(alpha, body);
  REQUIRE(mu->getFunctor() == "μ");
}

TEST_CASE("TopMu: Term interface - arity", "[TopType][Term]") {
  auto node = std::make_unique<ASTNumberExpr>(42);
  auto alpha = std::make_shared<TopAlpha>(node.get(), "0");
  auto body = std::make_shared<TopRef>(alpha);
  auto mu = std::make_shared<TopMu>(alpha, body);
  REQUIRE(mu->arity() == 2);
}

TEST_CASE("TopMu: Term interface - getSubterms", "[TopType][Term]") {
  auto node = std::make_unique<ASTNumberExpr>(42);
  auto alpha = std::make_shared<TopAlpha>(node.get(), "0");
  auto body = std::make_shared<TopRef>(alpha);
  auto mu = std::make_shared<TopMu>(alpha, body);
  REQUIRE_THROWS_AS(mu->getChildTypes(), InternalError);
}

TEST_CASE("TopMu: Term interface - withSubterms", "[TopType][Term]") {
  auto node = std::make_unique<ASTNumberExpr>(42);
  auto alpha = std::make_shared<TopAlpha>(node.get(), "0");
  auto body = std::make_shared<TopRef>(alpha);
  auto mu = std::make_shared<TopMu>(alpha, body);
  REQUIRE_THROWS_AS(mu->withChildTypes({}), InternalError);
}

// ==================== Cross-type equals ====================

TEST_CASE("Term equals: different types are not equal", "[TopType][Term]") {
  auto intType = std::make_shared<TopInt>();
  auto ptrType = std::make_shared<TopRef>(intType);
  REQUIRE(*intType != *ptrType);
  REQUIRE(*ptrType != *intType);
}

TEST_CASE("Term equals: TopRef with same inner are equal", "[TopType][Term]") {
  auto inner1 = std::make_shared<TopInt>();
  auto inner2 = std::make_shared<TopInt>();
  auto ref1 = std::make_shared<TopRef>(inner1);
  auto ref2 = std::make_shared<TopRef>(inner2);
  REQUIRE(*ref1 == *ref2);
}

// ==================== matchesFunctor (via getFunctor/arity) ====================

TEST_CASE("Term matchesFunctor: same functor and arity", "[TopType][Term]") {
  auto t1 = std::make_shared<TopInt>();
  auto t2 = std::make_shared<TopInt>();
  REQUIRE(t1->getFunctor() == t2->getFunctor());
  REQUIRE(t1->arity() == t2->arity());
}

TEST_CASE("Term matchesFunctor: different functor", "[TopType][Term]") {
  auto t1 = std::make_shared<TopInt>();
  auto t2 = std::make_shared<TopRef>(t1);
  REQUIRE(t1->getFunctor() != t2->getFunctor());
}

TEST_CASE("Term matchesFunctor: same functor different arity", "[TopType][Term]") {
  auto intType = std::make_shared<TopInt>();
  auto func1 = std::make_shared<TopFunction>(
      std::vector<std::shared_ptr<TopType>>{intType}, intType);
  auto func2 = std::make_shared<TopFunction>(
      std::vector<std::shared_ptr<TopType>>{intType, intType}, intType);
  // Both have functor "->", but different arity (2 vs 3)
  REQUIRE(func1->getFunctor() == func2->getFunctor());
  REQUIRE(func1->arity() != func2->arity());
}

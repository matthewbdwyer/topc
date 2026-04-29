#include "TipInt.h"
#include "TipVar.h"
#include "TipAlpha.h"
#include "TipAbsentField.h"
#include "TipRef.h"
#include "TipFunction.h"
#include "TipRecord.h"
#include "TipMu.h"
#include "InternalError.h"
#include "ASTNumberExpr.h"
#include "ASTVariableExpr.h"

#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <string>

// ==================== TipInt Term Interface ====================

TEST_CASE("TipInt: Term interface - isVariable", "[TipType][Term]") {
  auto t = std::make_shared<TipInt>();
  REQUIRE_FALSE(t->isVariable());
}

TEST_CASE("TipInt: Term interface - getFunctor", "[TipType][Term]") {
  auto t = std::make_shared<TipInt>();
  REQUIRE(t->getFunctor() == "int");
}

TEST_CASE("TipInt: Term interface - arity", "[TipType][Term]") {
  auto t = std::make_shared<TipInt>();
  REQUIRE(t->arity() == 0);
}

TEST_CASE("TipInt: Term interface - getSubterms", "[TipType][Term]") {
  auto t = std::make_shared<TipInt>();
  REQUIRE(t->getChildTypes().empty());
}

TEST_CASE("TipInt: Term interface - withSubterms empty", "[TipType][Term]") {
  auto t = std::make_shared<TipInt>();
  auto t2 = t->withChildTypes({});
  REQUIRE(t2->getFunctor() == "int");
  REQUIRE(*t == *t2);
}

TEST_CASE("TipInt: Term interface - withSubterms throws on non-empty", "[TipType][Term]") {
  auto t = std::make_shared<TipInt>();
  REQUIRE_THROWS_AS(t->withChildTypes({std::make_shared<TipInt>()}), std::invalid_argument);
}

TEST_CASE("TipInt: Term interface - toString", "[TipType][Term]") {
  auto t = std::make_shared<TipInt>();
  REQUIRE(t->toString() == "int");
}

TEST_CASE("TipInt: Term interface - equals", "[TipType][Term]") {
  auto t1 = std::make_shared<TipInt>();
  auto t2 = std::make_shared<TipInt>();
  REQUIRE(*t1 == *t2);
  REQUIRE(*t2 == *t1);
}

// ==================== TipVar Term Interface ====================

TEST_CASE("TipVar: Term interface - isVariable", "[TipType][Term]") {
  auto node = std::make_unique<ASTNumberExpr>(42);
  auto t = std::make_shared<TipVar>(node.get());
  REQUIRE(t->isVariable());
}

TEST_CASE("TipVar: Term interface - getFunctor contains address", "[TipType][Term]") {
  auto node = std::make_unique<ASTNumberExpr>(42);
  auto t = std::make_shared<TipVar>(node.get());
  std::string functor = t->getFunctor();
  REQUIRE(functor.find("var@") == 0);
}

TEST_CASE("TipVar: Term interface - arity", "[TipType][Term]") {
  auto node = std::make_unique<ASTNumberExpr>(42);
  auto t = std::make_shared<TipVar>(node.get());
  REQUIRE(t->arity() == 0);
}

TEST_CASE("TipVar: Term interface - getSubterms", "[TipType][Term]") {
  auto node = std::make_unique<ASTNumberExpr>(42);
  auto t = std::make_shared<TipVar>(node.get());
  REQUIRE(t->getChildTypes().empty());
}

TEST_CASE("TipVar: Term interface - withSubterms preserves node", "[TipType][Term]") {
  auto node = std::make_unique<ASTNumberExpr>(42);
  auto t = std::make_shared<TipVar>(node.get());
  auto t2 = std::dynamic_pointer_cast<TipVar>(t->withChildTypes({}));
  REQUIRE(t2 != nullptr);
  REQUIRE(t2->getNode() == node.get());
}

TEST_CASE("TipVar: Term interface - different nodes have different functors", "[TipType][Term]") {
  auto node1 = std::make_unique<ASTNumberExpr>(1);
  auto node2 = std::make_unique<ASTNumberExpr>(2);
  auto t1 = std::make_shared<TipVar>(node1.get());
  auto t2 = std::make_shared<TipVar>(node2.get());
  REQUIRE(t1->getFunctor() != t2->getFunctor());
}

// ==================== TipAlpha Term Interface ====================

TEST_CASE("TipAlpha: Term interface - isVariable is FALSE", "[TipType][Term]") {
  auto node = std::make_unique<ASTNumberExpr>(42);
  auto alpha = std::make_shared<TipAlpha>(node.get(), "0");
  REQUIRE_FALSE(alpha->isVariable());  // Critical: TipAlpha is NOT a unification variable
}

TEST_CASE("TipAlpha: Term interface - getFunctor includes name", "[TipType][Term]") {
  auto node = std::make_unique<ASTNumberExpr>(42);
  auto alpha = std::make_shared<TipAlpha>(node.get(), "0");
  REQUIRE(alpha->getFunctor() == "α0");
}

TEST_CASE("TipAlpha: Term interface - arity", "[TipType][Term]") {
  auto node = std::make_unique<ASTNumberExpr>(42);
  auto alpha = std::make_shared<TipAlpha>(node.get(), "0");
  REQUIRE(alpha->arity() == 0);
}

TEST_CASE("TipAlpha: Term interface - getSubterms", "[TipType][Term]") {
  auto node = std::make_unique<ASTNumberExpr>(42);
  auto alpha = std::make_shared<TipAlpha>(node.get(), "0");
  REQUIRE(alpha->getChildTypes().empty());
}

// ==================== TipAbsentField Term Interface ====================

TEST_CASE("TipAbsentField: Term interface - isVariable", "[TipType][Term]") {
  auto t = std::make_shared<TipAbsentField>();
  REQUIRE_FALSE(t->isVariable());
}

TEST_CASE("TipAbsentField: Term interface - getFunctor", "[TipType][Term]") {
  auto t = std::make_shared<TipAbsentField>();
  REQUIRE(t->getFunctor() == "absent");
}

TEST_CASE("TipAbsentField: Term interface - arity", "[TipType][Term]") {
  auto t = std::make_shared<TipAbsentField>();
  REQUIRE(t->arity() == 0);
}

// ==================== TipRef Term Interface ====================

TEST_CASE("TipRef: Term interface - isVariable", "[TipType][Term]") {
  auto inner = std::make_shared<TipInt>();
  auto t = std::make_shared<TipRef>(inner);
  REQUIRE_FALSE(t->isVariable());
}

TEST_CASE("TipRef: Term interface - getFunctor", "[TipType][Term]") {
  auto inner = std::make_shared<TipInt>();
  auto t = std::make_shared<TipRef>(inner);
  REQUIRE(t->getFunctor() == "ptr");
}

TEST_CASE("TipRef: Term interface - arity", "[TipType][Term]") {
  auto inner = std::make_shared<TipInt>();
  auto t = std::make_shared<TipRef>(inner);
  REQUIRE(t->arity() == 1);
}

TEST_CASE("TipRef: Term interface - getSubterms", "[TipType][Term]") {
  auto inner = std::make_shared<TipInt>();
  auto t = std::make_shared<TipRef>(inner);
  auto subs = t->getChildTypes();
  REQUIRE(subs.size() == 1);
  REQUIRE(subs[0]->getFunctor() == "int");
}

TEST_CASE("TipRef: Term interface - withSubterms", "[TipType][Term]") {
  auto inner = std::make_shared<TipInt>();
  auto t = std::make_shared<TipRef>(inner);
  auto newInner = std::make_shared<TipAbsentField>();
  auto t2 = t->withChildTypes({newInner});
  REQUIRE(t2->getFunctor() == "ptr");
  auto subs = t2->getChildTypes();
  REQUIRE(subs.size() == 1);
  REQUIRE(subs[0]->getFunctor() == "absent");
}

TEST_CASE("TipRef: Term interface - withSubterms wrong count throws", "[TipType][Term]") {
  auto inner = std::make_shared<TipInt>();
  auto t = std::make_shared<TipRef>(inner);
  REQUIRE_THROWS_AS(t->withChildTypes({}), std::invalid_argument);
  REQUIRE_THROWS_AS(t->withChildTypes({inner, inner}), std::invalid_argument);
}

// ==================== TipFunction Term Interface ====================

TEST_CASE("TipFunction: Term interface - isVariable", "[TipType][Term]") {
  auto intType = std::make_shared<TipInt>();
  auto func = std::make_shared<TipFunction>(
      std::vector<std::shared_ptr<TipType>>{intType}, intType);
  REQUIRE_FALSE(func->isVariable());
}

TEST_CASE("TipFunction: Term interface - getFunctor", "[TipType][Term]") {
  auto intType = std::make_shared<TipInt>();
  auto func = std::make_shared<TipFunction>(
      std::vector<std::shared_ptr<TipType>>{intType}, intType);
  REQUIRE(func->getFunctor() == "->");
}

TEST_CASE("TipFunction: Term interface - arity with one param", "[TipType][Term]") {
  auto intType = std::make_shared<TipInt>();
  auto func = std::make_shared<TipFunction>(
      std::vector<std::shared_ptr<TipType>>{intType}, intType);
  REQUIRE(func->arity() == 2);  // 1 param + 1 return
}

TEST_CASE("TipFunction: Term interface - arity with multiple params", "[TipType][Term]") {
  auto intType = std::make_shared<TipInt>();
  auto func = std::make_shared<TipFunction>(
      std::vector<std::shared_ptr<TipType>>{intType, intType, intType}, intType);
  REQUIRE(func->arity() == 4);  // 3 params + 1 return
}

TEST_CASE("TipFunction: Term interface - arity with no params", "[TipType][Term]") {
  auto intType = std::make_shared<TipInt>();
  auto func = std::make_shared<TipFunction>(
      std::vector<std::shared_ptr<TipType>>{}, intType);
  REQUIRE(func->arity() == 1);  // 0 params + 1 return
}

TEST_CASE("TipFunction: Term interface - getSubterms", "[TipType][Term]") {
  auto intType = std::make_shared<TipInt>();
  auto absent = std::make_shared<TipAbsentField>();
  auto func = std::make_shared<TipFunction>(
      std::vector<std::shared_ptr<TipType>>{intType, absent}, intType);
  auto subs = func->getChildTypes();
  REQUIRE(subs.size() == 3);
  REQUIRE(subs[0]->getFunctor() == "int");
  REQUIRE(subs[1]->getFunctor() == "absent");
  REQUIRE(subs[2]->getFunctor() == "int");
}

TEST_CASE("TipFunction: Term interface - withSubterms", "[TipType][Term]") {
  auto intType = std::make_shared<TipInt>();
  auto func = std::make_shared<TipFunction>(
      std::vector<std::shared_ptr<TipType>>{intType}, intType);
  auto absent = std::make_shared<TipAbsentField>();
  auto func2 = func->withChildTypes({absent, absent});
  auto subs = func2->getChildTypes();
  REQUIRE(subs.size() == 2);
  REQUIRE(subs[0]->getFunctor() == "absent");
  REQUIRE(subs[1]->getFunctor() == "absent");
}

// ==================== TipRecord Term Interface ====================

TEST_CASE("TipRecord: Term interface - isVariable", "[TipType][Term]") {
  auto intType = std::make_shared<TipInt>();
  auto rec = std::make_shared<TipRecord>(
      std::vector<std::shared_ptr<TipType>>{intType, intType},
      std::vector<std::string>{"x", "y"});
  REQUIRE_FALSE(rec->isVariable());
}

TEST_CASE("TipRecord: Term interface - getFunctor encodes sorted field names", "[TipType][Term]") {
  auto intType = std::make_shared<TipInt>();
  // Fields in unsorted order
  auto rec = std::make_shared<TipRecord>(
      std::vector<std::shared_ptr<TipType>>{intType, intType},
      std::vector<std::string>{"y", "x"});
  // Functor should have sorted names
  REQUIRE(rec->getFunctor() == "record{x,y}");
}

TEST_CASE("TipRecord: Term interface - arity", "[TipType][Term]") {
  auto intType = std::make_shared<TipInt>();
  auto rec = std::make_shared<TipRecord>(
      std::vector<std::shared_ptr<TipType>>{intType, intType, intType},
      std::vector<std::string>{"a", "b", "c"});
  REQUIRE(rec->arity() == 3);
}

TEST_CASE("TipRecord: Term interface - getSubterms", "[TipType][Term]") {
  auto intType = std::make_shared<TipInt>();
  auto absent = std::make_shared<TipAbsentField>();
  auto rec = std::make_shared<TipRecord>(
      std::vector<std::shared_ptr<TipType>>{intType, absent},
      std::vector<std::string>{"x", "y"});
  auto subs = rec->getChildTypes();
  REQUIRE(subs.size() == 2);
  REQUIRE(subs[0]->getFunctor() == "int");
  REQUIRE(subs[1]->getFunctor() == "absent");
}

TEST_CASE("TipRecord: Term interface - withSubterms preserves field names", "[TipType][Term]") {
  auto intType = std::make_shared<TipInt>();
  auto rec = std::make_shared<TipRecord>(
      std::vector<std::shared_ptr<TipType>>{intType, intType},
      std::vector<std::string>{"x", "y"});
  auto absent = std::make_shared<TipAbsentField>();
  auto rec2 = std::dynamic_pointer_cast<TipRecord>(
      rec->withChildTypes({absent, absent}));
  REQUIRE(rec2 != nullptr);
  REQUIRE(rec2->getFunctor() == "record{x,y}");
  auto subs = rec2->getChildTypes();
  REQUIRE(subs[0]->getFunctor() == "absent");
  REQUIRE(subs[1]->getFunctor() == "absent");
}

// ==================== TipMu Term Interface ====================

TEST_CASE("TipMu: Term interface - isVariable", "[TipType][Term]") {
  auto node = std::make_unique<ASTNumberExpr>(42);
  auto alpha = std::make_shared<TipAlpha>(node.get(), "0");
  auto body = std::make_shared<TipRef>(alpha);
  auto mu = std::make_shared<TipMu>(alpha, body);
  REQUIRE_FALSE(mu->isVariable());
}

TEST_CASE("TipMu: Term interface - getFunctor", "[TipType][Term]") {
  auto node = std::make_unique<ASTNumberExpr>(42);
  auto alpha = std::make_shared<TipAlpha>(node.get(), "0");
  auto body = std::make_shared<TipRef>(alpha);
  auto mu = std::make_shared<TipMu>(alpha, body);
  REQUIRE(mu->getFunctor() == "μ");
}

TEST_CASE("TipMu: Term interface - arity", "[TipType][Term]") {
  auto node = std::make_unique<ASTNumberExpr>(42);
  auto alpha = std::make_shared<TipAlpha>(node.get(), "0");
  auto body = std::make_shared<TipRef>(alpha);
  auto mu = std::make_shared<TipMu>(alpha, body);
  REQUIRE(mu->arity() == 2);
}

TEST_CASE("TipMu: Term interface - getSubterms", "[TipType][Term]") {
  auto node = std::make_unique<ASTNumberExpr>(42);
  auto alpha = std::make_shared<TipAlpha>(node.get(), "0");
  auto body = std::make_shared<TipRef>(alpha);
  auto mu = std::make_shared<TipMu>(alpha, body);
  REQUIRE_THROWS_AS(mu->getChildTypes(), InternalError);
}

TEST_CASE("TipMu: Term interface - withSubterms", "[TipType][Term]") {
  auto node = std::make_unique<ASTNumberExpr>(42);
  auto alpha = std::make_shared<TipAlpha>(node.get(), "0");
  auto body = std::make_shared<TipRef>(alpha);
  auto mu = std::make_shared<TipMu>(alpha, body);
  REQUIRE_THROWS_AS(mu->withChildTypes({}), InternalError);
}

// ==================== Cross-type equals ====================

TEST_CASE("Term equals: different types are not equal", "[TipType][Term]") {
  auto intType = std::make_shared<TipInt>();
  auto absent = std::make_shared<TipAbsentField>();
  REQUIRE(*intType != *absent);
  REQUIRE(*absent != *intType);
}

TEST_CASE("Term equals: TipRef with same inner are equal", "[TipType][Term]") {
  auto inner1 = std::make_shared<TipInt>();
  auto inner2 = std::make_shared<TipInt>();
  auto ref1 = std::make_shared<TipRef>(inner1);
  auto ref2 = std::make_shared<TipRef>(inner2);
  REQUIRE(*ref1 == *ref2);
}

// ==================== matchesFunctor (via getFunctor/arity) ====================

TEST_CASE("Term matchesFunctor: same functor and arity", "[TipType][Term]") {
  auto t1 = std::make_shared<TipInt>();
  auto t2 = std::make_shared<TipInt>();
  REQUIRE(t1->getFunctor() == t2->getFunctor());
  REQUIRE(t1->arity() == t2->arity());
}

TEST_CASE("Term matchesFunctor: different functor", "[TipType][Term]") {
  auto t1 = std::make_shared<TipInt>();
  auto t2 = std::make_shared<TipAbsentField>();
  REQUIRE(t1->getFunctor() != t2->getFunctor());
}

TEST_CASE("Term matchesFunctor: same functor different arity", "[TipType][Term]") {
  auto intType = std::make_shared<TipInt>();
  auto func1 = std::make_shared<TipFunction>(
      std::vector<std::shared_ptr<TipType>>{intType}, intType);
  auto func2 = std::make_shared<TipFunction>(
      std::vector<std::shared_ptr<TipType>>{intType, intType}, intType);
  // Both have functor "->", but different arity (2 vs 3)
  REQUIRE(func1->getFunctor() == func2->getFunctor());
  REQUIRE(func1->arity() != func2->arity());
}

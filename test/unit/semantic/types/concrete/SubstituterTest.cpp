#include "Substituter.h"
#include "TipInt.h"
#include "TipVar.h"
#include "TipAlpha.h"
#include "TipRef.h"
#include "TipFunction.h"
#include "TipRecord.h"
#include "TipMu.h"
#include "ASTNumberExpr.h"

#include <catch2/catch_test_macros.hpp>
#include <memory>

// Helpers
static ASTNumberExpr nodeA(1);
static ASTNumberExpr nodeB(2);
static ASTNumberExpr nodeC(3);

TEST_CASE("Substituter: nullary type unchanged", "[Substituter]") {
  auto target = std::make_shared<TipVar>(&nodeA);
  auto intType = std::make_shared<TipInt>();
  auto sub = std::make_shared<TipInt>();

  auto result = Substituter::substitute(intType.get(), target.get(), sub);
  REQUIRE(result->getFunctor() == "int");
}

TEST_CASE("Substituter: target variable is replaced", "[Substituter]") {
  auto target = std::make_shared<TipVar>(&nodeA);
  auto sub = std::make_shared<TipInt>();

  auto result = Substituter::substitute(target.get(), target.get(), sub);
  REQUIRE(result->getFunctor() == "int");
}

TEST_CASE("Substituter: non-target variable is unchanged", "[Substituter]") {
  auto target = std::make_shared<TipVar>(&nodeA);
  auto other  = std::make_shared<TipVar>(&nodeB);
  auto sub    = std::make_shared<TipInt>();

  auto result = Substituter::substitute(other.get(), target.get(), sub);
  // Result should still be a TipVar wrapping nodeB
  auto resultVar = std::dynamic_pointer_cast<TipVar>(result);
  REQUIRE(resultVar != nullptr);
  REQUIRE(resultVar->getNode() == &nodeB);
}

TEST_CASE("Substituter: compound type — one occurrence replaced", "[Substituter]") {
  auto x = std::make_shared<TipVar>(&nodeA);
  auto ref = std::make_shared<TipRef>(x);
  auto sub = std::make_shared<TipInt>();

  auto result = Substituter::substitute(ref.get(), x.get(), sub);
  REQUIRE(result->getFunctor() == "ptr");
  auto subs = result->getChildTypes();
  REQUIRE(subs.size() == 1);
  REQUIRE(subs[0]->getFunctor() == "int");
}

TEST_CASE("Substituter: multiple occurrences all replaced", "[Substituter]") {
  auto x = std::make_shared<TipVar>(&nodeA);
  auto sub = std::make_shared<TipInt>();
  auto func = std::make_shared<TipFunction>(
      std::vector<std::shared_ptr<TipType>>{x}, x);

  auto result = Substituter::substitute(func.get(), x.get(), sub);
  REQUIRE(result->getFunctor() == "->");
  auto subs = result->getChildTypes();
  REQUIRE(subs.size() == 2);  // 1 param + return
  REQUIRE(subs[0]->getFunctor() == "int");  // param replaced
  REQUIRE(subs[1]->getFunctor() == "int");  // return replaced
}

TEST_CASE("Substituter: compound substitution is preserved", "[Substituter]") {
  auto x = std::make_shared<TipVar>(&nodeA);
  auto inner = std::make_shared<TipInt>();
  auto sub = std::make_shared<TipRef>(inner);  // sub is Ref(int)

  auto result = Substituter::substitute(x.get(), x.get(), sub);
  // Result should be Ref(int)
  REQUIRE(result->getFunctor() == "ptr");
  auto subs = result->getChildTypes();
  REQUIRE(subs[0]->getFunctor() == "int");
}

TEST_CASE("Substituter: TipRecord — field names preserved after substitution", "[Substituter]") {
  auto x = std::make_shared<TipVar>(&nodeA);
  auto y = std::make_shared<TipVar>(&nodeB);
  auto sub = std::make_shared<TipInt>();

  auto rec = std::make_shared<TipRecord>(
      std::vector<std::shared_ptr<TipType>>{x, y},
      std::vector<std::string>{"f", "g"});

  auto result = Substituter::substitute(rec.get(), x.get(), sub);

  // Field names should be {f, g} sorted → record{f,g}
  REQUIRE(result->getFunctor() == "record{f,g}");
  auto subs = result->getChildTypes();
  REQUIRE(subs.size() == 2);
  REQUIRE(subs[0]->getFunctor() == "int");    // f: x → int
  auto yResult = std::dynamic_pointer_cast<TipVar>(subs[1]);
  REQUIRE(yResult != nullptr);
  REQUIRE(yResult->getNode() == &nodeB);       // g: y unchanged
}

TEST_CASE("Substituter: substituted value is a fresh copy", "[Substituter]") {
  auto x = std::make_shared<TipVar>(&nodeA);
  auto sub = std::make_shared<TipInt>();

  auto result = Substituter::substitute(x.get(), x.get(), sub);
  // Copier::copy produced a fresh allocation, not the same pointer
  REQUIRE(result.get() != sub.get());
}

TEST_CASE("Substituter: TipMu — only free variable is replaced", "[Substituter]") {
  // TipMu(v, TipRef(x))  where v != x
  // Substituting x → int should leave v unchanged and replace x
  auto v = std::make_shared<TipVar>(&nodeA);
  auto x = std::make_shared<TipVar>(&nodeB);
  auto body = std::make_shared<TipRef>(x);
  auto mu = std::make_shared<TipMu>(v, body);

  auto sub = std::make_shared<TipInt>();
  auto result = Substituter::substitute(mu.get(), x.get(), sub);

  // Result is TipMu
  REQUIRE(result->getFunctor() == "μ");
  auto muResult = std::dynamic_pointer_cast<TipMu>(result);
  REQUIRE(muResult != nullptr);
  // Binding variable still a TipVar wrapping nodeA
  REQUIRE(muResult->getV()->getNode() == &nodeA);
  // Body is Ref(int)
  REQUIRE(muResult->getT()->getFunctor() == "ptr");
  auto refBody = std::dynamic_pointer_cast<TipRef>(muResult->getT());
  REQUIRE(refBody != nullptr);
  REQUIRE(refBody->getReferencedType()->getFunctor() == "int");
}

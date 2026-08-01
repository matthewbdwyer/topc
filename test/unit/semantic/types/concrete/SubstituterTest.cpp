#include "Substituter.h"
#include "TopInt.h"
#include "TopVar.h"
#include "TopAlpha.h"
#include "TopOwningRef.h"
#include "TopFunction.h"
#include "TopMu.h"
#include "ASTNumberExpr.h"

#include <catch2/catch_test_macros.hpp>
#include <memory>

// Helpers
static ASTNumberExpr nodeA(1);
static ASTNumberExpr nodeB(2);
static ASTNumberExpr nodeC(3);

TEST_CASE("Substituter: nullary type unchanged", "[Substituter]") {
  auto target = std::make_shared<TopVar>(&nodeA);
  auto intType = std::make_shared<TopInt>();
  auto sub = std::make_shared<TopInt>();

  auto result = Substituter::substitute(intType.get(), target.get(), sub);
  REQUIRE(result->getFunctor() == "int");
}

TEST_CASE("Substituter: target variable is replaced", "[Substituter]") {
  auto target = std::make_shared<TopVar>(&nodeA);
  auto sub = std::make_shared<TopInt>();

  auto result = Substituter::substitute(target.get(), target.get(), sub);
  REQUIRE(result->getFunctor() == "int");
}

TEST_CASE("Substituter: non-target variable is unchanged", "[Substituter]") {
  auto target = std::make_shared<TopVar>(&nodeA);
  auto other  = std::make_shared<TopVar>(&nodeB);
  auto sub    = std::make_shared<TopInt>();

  auto result = Substituter::substitute(other.get(), target.get(), sub);
  // Result should still be a TopVar wrapping nodeB
  auto resultVar = std::dynamic_pointer_cast<TopVar>(result);
  REQUIRE(resultVar != nullptr);
  REQUIRE(resultVar->getNode() == &nodeB);
}

TEST_CASE("Substituter: compound type — one occurrence replaced", "[Substituter]") {
  auto x = std::make_shared<TopVar>(&nodeA);
  auto ref = std::make_shared<TopOwningRef>(x);
  auto sub = std::make_shared<TopInt>();

  auto result = Substituter::substitute(ref.get(), x.get(), sub);
  REQUIRE(result->getFunctor() == "ref");
  auto subs = result->getChildTypes();
  REQUIRE(subs.size() == 2);
  REQUIRE(subs[0]->getFunctor() == "ownmode");
  REQUIRE(subs[1]->getFunctor() == "int");
}

TEST_CASE("Substituter: multiple occurrences all replaced", "[Substituter]") {
  auto x = std::make_shared<TopVar>(&nodeA);
  auto sub = std::make_shared<TopInt>();
  auto func = std::make_shared<TopFunction>(
      std::vector<std::shared_ptr<TopType>>{x}, x);

  auto result = Substituter::substitute(func.get(), x.get(), sub);
  REQUIRE(result->getFunctor() == "->");
  auto subs = result->getChildTypes();
  REQUIRE(subs.size() == 2);  // 1 param + return
  REQUIRE(subs[0]->getFunctor() == "int");  // param replaced
  REQUIRE(subs[1]->getFunctor() == "int");  // return replaced
}

TEST_CASE("Substituter: compound substitution is preserved", "[Substituter]") {
  auto x = std::make_shared<TopVar>(&nodeA);
  auto inner = std::make_shared<TopInt>();
  auto sub = std::make_shared<TopOwningRef>(inner);  // sub is Ref(int)

  auto result = Substituter::substitute(x.get(), x.get(), sub);
  // Result should be Ref(Own, int)
  REQUIRE(result->getFunctor() == "ref");
  auto subs = result->getChildTypes();
  REQUIRE(subs.size() == 2);
  REQUIRE(subs[0]->getFunctor() == "ownmode");
  REQUIRE(subs[1]->getFunctor() == "int");
}

TEST_CASE("Substituter: substituted value is a fresh copy", "[Substituter]") {
  auto x = std::make_shared<TopVar>(&nodeA);
  auto sub = std::make_shared<TopInt>();

  auto result = Substituter::substitute(x.get(), x.get(), sub);
  // Copier::copy produced a fresh allocation, not the same pointer
  REQUIRE(result.get() != sub.get());
}

TEST_CASE("Substituter: TopMu — only free variable is replaced", "[Substituter]") {
  // TopMu(v, TopOwningRef(x))  where v != x
  // Substituting x → int should leave v unchanged and replace x
  auto v = std::make_shared<TopVar>(&nodeA);
  auto x = std::make_shared<TopVar>(&nodeB);
  auto body = std::make_shared<TopOwningRef>(x);
  auto mu = std::make_shared<TopMu>(v, body);

  auto sub = std::make_shared<TopInt>();
  auto result = Substituter::substitute(mu.get(), x.get(), sub);

  // Result is TopMu
  REQUIRE(result->getFunctor() == "μ");
  auto muResult = std::dynamic_pointer_cast<TopMu>(result);
  REQUIRE(muResult != nullptr);
  // Binding variable still a TopVar wrapping nodeA
  REQUIRE(muResult->getV()->getNode() == &nodeA);
  // Body is Ref(Own, int)
  REQUIRE(muResult->getT()->getFunctor() == "ref");
  auto refBody = std::dynamic_pointer_cast<TopOwningRef>(muResult->getT());
  REQUIRE(refBody != nullptr);
  REQUIRE(refBody->getReferencedType()->getFunctor() == "int");
}

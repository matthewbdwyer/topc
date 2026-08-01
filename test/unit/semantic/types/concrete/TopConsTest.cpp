#include "TopCons.h"
#include "TopInt.h"
#include "TopOwningRef.h"
#include "TopTypeVisitor.h"
#include "TopVar.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("TopCons: Test doMatch considers arity",
          "[TopCons]") {
  auto tipInt = std::make_shared<TopInt>();

  std::vector<std::shared_ptr<TopType>> params0{};
  std::vector<std::shared_ptr<TopType>> params1{tipInt};

  // Function with no params
  auto tipFunction1 = std::make_shared<TopFunction>(params0, tipInt);

  // Function with one param
  auto tipFunction2 = std::make_shared<TopFunction>(params1, tipInt);

  REQUIRE_FALSE(tipFunction1->doMatch(tipFunction2.get()));
  REQUIRE_FALSE(tipFunction2->doMatch(tipFunction1.get()));
}

TEST_CASE("TopCons: Test doMatch considers constructor type",
          "[TopCons]") {
  std::vector<std::shared_ptr<TopType>> params{};

  auto tipInt = std::make_shared<TopInt>();

  // Ref is cons with 1 argument
  auto tipRef = std::make_shared<TopOwningRef>(tipInt);

  // Function with no params is cons with one argument
  auto tipFunction = std::make_shared<TopFunction>(params, tipInt);

  REQUIRE_FALSE(tipRef->doMatch(tipFunction.get()));
  REQUIRE_FALSE(tipFunction->doMatch(tipRef.get()));
}

TEST_CASE("TopCons: Test doMatch only works on TopCons",
          "[TopCons]") {
  auto tipInt = std::make_shared<TopInt>();
  auto tipRef = std::make_shared<TopOwningRef>(tipInt);

  REQUIRE_FALSE(tipRef->doMatch(tipInt.get()));
}

TEST_CASE("TopCons: doMatch returns true for matching constructors", "[TopCons]") {
  auto tipInt = std::make_shared<TopInt>();

  // TopInt matches TopInt
  REQUIRE(tipInt->doMatch(tipInt.get()));

  // TopOwningRef matches TopOwningRef
  auto ref1 = std::make_shared<TopOwningRef>(tipInt);
  auto ref2 = std::make_shared<TopOwningRef>(tipInt);
  REQUIRE(ref1->doMatch(ref2.get()));

  // TopFunction([int], int) matches TopFunction([int], int)
  auto func1 = std::make_shared<TopFunction>(
      std::vector<std::shared_ptr<TopType>>{tipInt}, tipInt);
  auto func2 = std::make_shared<TopFunction>(
      std::vector<std::shared_ptr<TopType>>{tipInt}, tipInt);
  REQUIRE(func1->doMatch(func2.get()));
}

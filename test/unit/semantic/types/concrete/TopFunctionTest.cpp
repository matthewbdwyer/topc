#include "TopFunction.h"
#include "TopInt.h"
#include "TopOwningRef.h"

#include <catch2/catch_test_macros.hpp>

#include <iostream>
#include <sstream>

TEST_CASE("TopFunction: Test getters",
          "[TopFunction]") {
  std::vector<std::shared_ptr<TopType>> params{std::make_shared<TopInt>(),
                                               std::make_shared<TopInt>()};
  auto ret = std::make_shared<TopOwningRef>(std::make_shared<TopInt>());
  TopFunction tipFunction(params, ret);

  REQUIRE(2 == tipFunction.getParamTypes().size());
  REQUIRE(*ret == *tipFunction.getReturnType());
}

TEST_CASE("TopFunction: Test equality",
          "[TopFunction]") {
  std::vector<std::shared_ptr<TopType>> paramsA{std::make_shared<TopInt>(),
                                                std::make_shared<TopInt>()};
  auto retA = std::make_shared<TopInt>();
  TopFunction tipFunctionA(paramsA, retA);

  SECTION("Equal when arguments and return value are of same type and length") {
    std::vector<std::shared_ptr<TopType>> paramsB{std::make_shared<TopInt>(),
                                                  std::make_shared<TopInt>()};
    auto retB = std::make_shared<TopInt>();
    TopFunction tipFunctionB(paramsB, retB);
    REQUIRE(tipFunctionA == tipFunctionB);
  }

  SECTION("Not equal when arguments differ by length") {
    std::vector<std::shared_ptr<TopType>> paramsB{std::make_shared<TopInt>()};
    auto retB = std::make_shared<TopInt>();
    TopFunction tipFunctionB(paramsB, retB);
    REQUIRE(tipFunctionA != tipFunctionB);
  }

  SECTION("Not equal when arguments differ by type") {
    std::vector<std::shared_ptr<TopType>> paramsB{
        std::make_shared<TopInt>(),
        std::make_shared<TopOwningRef>(std::make_shared<TopInt>())};
    auto retB = std::make_shared<TopInt>();
    TopFunction tipFunctionB(paramsB, retB);
    REQUIRE(tipFunctionA != tipFunctionB);
  }

  SECTION("Not equal when return values differ by type") {
    std::vector<std::shared_ptr<TopType>> paramsB{std::make_shared<TopInt>(),
                                                  std::make_shared<TopInt>()};
    auto retB = std::make_shared<TopOwningRef>(std::make_shared<TopInt>());
    TopFunction tipFunctionB(paramsB, retB);
    REQUIRE(tipFunctionA != tipFunctionB);
  }
}

TEST_CASE("TopFunction: Test output stream",
          "[TopFunction]") {
  std::vector<std::shared_ptr<TopType>> params{std::make_shared<TopInt>(),
                                               std::make_shared<TopInt>()};
  auto ret = std::make_shared<TopOwningRef>(std::make_shared<TopInt>());
  TopFunction tipFunction(params, ret);

  auto expectedValue = "(int,int) -> own&int";
  std::stringstream stream;
  stream << tipFunction;
  std::string actualValue = stream.str();

  REQUIRE(expectedValue == actualValue);
}

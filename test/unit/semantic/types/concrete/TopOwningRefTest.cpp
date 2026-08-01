#include "TopOwningRef.h"
#include "TopFunction.h"
#include "TopInt.h"

#include <catch2/catch_test_macros.hpp>

#include <sstream>

TEST_CASE("TopOwningRef: compares by referenced type", "[TopOwningRef]") {
  auto referencedType = std::make_shared<TopInt>();
  TopOwningRef owningRef(referencedType);

  SECTION("Equal when referenced types are the same") {
    auto referencedType = std::make_shared<TopInt>();
    TopOwningRef otherOwningRef(referencedType);
    REQUIRE(owningRef == otherOwningRef);
  }

  SECTION("Not equal when referenced types are different") {
    std::vector<std::shared_ptr<TopType>> params{std::make_shared<TopInt>(),
                                                 std::make_shared<TopInt>()};
    auto ret = std::make_shared<TopOwningRef>(std::make_shared<TopInt>());
    auto functionType = std::make_shared<TopFunction>(params, ret);
    TopOwningRef otherOwningRef(functionType);

    REQUIRE_FALSE(owningRef == otherOwningRef);
  }
}

TEST_CASE("TopOwningRef: arity includes mode and referenced type", "[TopOwningRef]") {
  auto referencedType = std::make_shared<TopInt>();
  TopOwningRef owningRef(referencedType);
  REQUIRE(2 == owningRef.arity());
}

TEST_CASE("TopOwningRef: exposes referenced type", "[TopOwningRef]") {
  std::vector<std::shared_ptr<TopType>> params{std::make_shared<TopInt>(),
                                               std::make_shared<TopInt>()};
  auto ret = std::make_shared<TopOwningRef>(std::make_shared<TopInt>());
  auto functionType = std::make_shared<TopFunction>(params, ret);
  TopOwningRef owningRef(functionType);

  REQUIRE(*functionType == *owningRef.getReferencedType());
}

TEST_CASE("TopOwningRef: prints own reference", "[TopOwningRef]") {
  std::vector<std::shared_ptr<TopType>> params{std::make_shared<TopInt>(),
                                               std::make_shared<TopInt>()};
  auto ret = std::make_shared<TopInt>();
  auto functionType = std::make_shared<TopFunction>(params, ret);
  TopOwningRef owningRef(functionType);

  auto expectedValue = "own&(int,int) -> int";
  std::stringstream stream;
  stream << owningRef;
  std::string actualValue = stream.str();

  REQUIRE(expectedValue == actualValue);
}
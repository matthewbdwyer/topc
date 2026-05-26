#include "TopRef.h"
#include "TopFunction.h"
#include "TopInt.h"

#include <catch2/catch_test_macros.hpp>

#include <sstream>

TEST_CASE("TopRef: Test TopRefs are compared by their underlying term",
          "[TopRef]") {
  auto term = std::make_shared<TopInt>();
  TopRef tipRef(term);

  SECTION("Equal when terms are the same") {
    auto term = std::make_shared<TopInt>();
    TopRef tipRef2(term);
    REQUIRE(tipRef == tipRef2);
  }

  SECTION("Not equal when terms are different") {
    std::vector<std::shared_ptr<TopType>> params{std::make_shared<TopInt>(),
                                                 std::make_shared<TopInt>()};
    auto ret = std::make_shared<TopRef>(std::make_shared<TopInt>());
    auto tipFunction = std::make_shared<TopFunction>(params, ret);
    TopRef tipRef2(tipFunction);

    REQUIRE_FALSE(tipRef == tipRef2);
  }
}

TEST_CASE("TopRef: Test arity is one",
          "[TopRef]") {
  auto term = std::make_shared<TopInt>();
  TopRef tipRef(term);
  REQUIRE(1 == tipRef.arity());
}

TEST_CASE("TopRef: Test getter",
          "[TopRef]") {
  std::vector<std::shared_ptr<TopType>> params{std::make_shared<TopInt>(),
                                               std::make_shared<TopInt>()};
  auto ret = std::make_shared<TopRef>(std::make_shared<TopInt>());
  auto tipFunction = std::make_shared<TopFunction>(params, ret);
  TopRef tipRef(tipFunction);

  REQUIRE(*tipFunction == *tipRef.getReferencedType());
}

TEST_CASE("TopRef: Test output stream",
          "[TopRef]") {
  std::vector<std::shared_ptr<TopType>> params{std::make_shared<TopInt>(),
                                               std::make_shared<TopInt>()};
  auto ret = std::make_shared<TopInt>();
  auto tipFunction = std::make_shared<TopFunction>(params, ret);
  TopRef tipRef(tipFunction);

  auto expectedValue = "\u2B61(int,int) -> int";
  std::stringstream stream;
  stream << tipRef;
  std::string actualValue = stream.str();

  REQUIRE(expectedValue == actualValue);
}

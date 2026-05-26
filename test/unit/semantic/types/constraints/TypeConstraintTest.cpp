#include "TypeConstraint.h"
#include "TopFunction.h"
#include "TopInt.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <sstream>
#include <vector>

TEST_CASE("TypeConstraint: Constraints are compared term-wise",
          "[TypeConstraint]") {
  std::vector<std::shared_ptr<TopType>> args;
  args.push_back(std::make_shared<TopInt>());
  auto function =
      std::make_shared<TopFunction>(args, std::make_shared<TopInt>());

  TypeConstraint constraint(function, function);
  TypeConstraint constraint2(function, function);
  REQUIRE(constraint == constraint2);
  REQUIRE_FALSE(constraint != constraint2);
}

TEST_CASE("TypeConstraint: Test output", "[TypeConstraint]") {
  std::vector<std::shared_ptr<TopType>> args;
  args.push_back(std::make_shared<TopInt>());
  auto function =
      std::make_shared<TopFunction>(args, std::make_shared<TopInt>());
  TypeConstraint constraint(function, function);

  std::stringstream sstream;
  sstream << constraint;
  REQUIRE_THAT(sstream.str(), Catch::Matchers::Matches("^.* = .*$"));
}

#include "TopInt.h"
#include "TopVar.h"

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>

TEST_CASE("TopInt: test TopInt is a TopCons",
          "[tip_int]") {
  TopInt t;
  REQUIRE_FALSE(nullptr == dynamic_cast<TopCons *>(&t));
}

TEST_CASE("TopInt: test TopInt is a TopType",
          "[tip_int]") {
  TopInt t;
  REQUIRE_FALSE(nullptr == dynamic_cast<TopType *>(&t));
}

TEST_CASE("TopInt: test args is empty", "[tip_int]") {
  TopInt t;
  REQUIRE(t.getArguments().empty());
}

TEST_CASE("TopInt: test toString returns int", "[tip_int]") {
  TopInt t;
  std::stringstream stream;
  stream << t;
  REQUIRE("int" == stream.str());
}

TEST_CASE("TopInt: test all TopInts are equal", "[tip_int]") {
  TopInt t1;
  TopInt t2;
  REQUIRE(t1 == t2);
}

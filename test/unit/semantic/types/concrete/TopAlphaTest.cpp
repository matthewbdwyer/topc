#include "TopAlpha.h"
#include "ASTNumberExpr.h"

#include <catch2/catch_test_macros.hpp>

#include <sstream>

TEST_CASE("TopAlpha: Test TopAlphas are compared by their underlying objects",
          "[TopAlpha]") {
  ASTNumberExpr num(13);
  TopAlpha tipAlphaA(&num);
  TopAlpha tipAlphaB(&num);
  REQUIRE(tipAlphaA == tipAlphaB);
}

TEST_CASE("TopAlpha: TopAlpha equality with context and names",
          "[TopAlpha]") {
  ASTNumberExpr num1(13);
  ASTNumberExpr num2(42);
  ASTNumberExpr num3(7);

  TopAlpha tipAlpha1(&num1);
  TopAlpha tipAlpha1foo(&num1, "foo");
  TopAlpha tipAlpha1bar(&num1, "bar");
  TopAlpha tipAlpha21foo(&num2, &num1, "foo");
  TopAlpha tipAlpha12bar(&num1, &num2, "bar");
  TopAlpha tipAlpha21foo2(&num2, &num1, "foo");
  TopAlpha tipAlpha13bar(&num1, &num3, "bar");
  TopAlpha tipAlpha21bar(&num2, &num1, "bar");

  REQUIRE_FALSE(tipAlpha1 == tipAlpha1bar);      // implicit name mismatch
  REQUIRE_FALSE(tipAlpha1bar == tipAlpha1foo);   // explicit name mismatch
  REQUIRE_FALSE(tipAlpha1bar == tipAlpha12bar);  // context mismatch
  REQUIRE_FALSE(tipAlpha21foo == tipAlpha21bar); // context match, name mismatch
  REQUIRE_FALSE(tipAlpha12bar == tipAlpha13bar); // context mismatch, name match
  REQUIRE(tipAlpha21foo == tipAlpha21foo2);
}

TEST_CASE("TopAlpha: Test getter",
          "[TopAlpha]") {
  ASTNumberExpr num1(13);
  ASTNumberExpr num2(42);
  TopAlpha tipAlphaA(&num1);
  TopAlpha tipAlphaB(&num2);

  auto node1 = dynamic_cast<ASTNumberExpr *>(tipAlphaA.getNode());
  auto node2 = dynamic_cast<ASTNumberExpr *>(tipAlphaB.getNode());
  REQUIRE(node1->getValue() == 13);
  REQUIRE(node2->getValue() == 42);
}

TEST_CASE("TopAlpha: Test output stream",
          "[TopAlpha]") {
  ASTNumberExpr num(13);
  TopAlpha tipAlphaA(&num);

  std::string expectedValueA("\u03B1<13@0:0>");

  std::stringstream stream;
  stream << tipAlphaA;
  std::string actualValueA = stream.str();

  REQUIRE(expectedValueA == actualValueA);
}

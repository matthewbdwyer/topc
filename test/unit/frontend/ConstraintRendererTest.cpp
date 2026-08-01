#include "ConstraintRenderer.h"

#include <catch2/catch_test_macros.hpp>

#include <sstream>

TEST_CASE("ConstraintRenderer: section labeling", "[ConstraintRenderer]") {
  std::vector<ConstraintRecord> records{{"type", 1, 2, "x = Int"}};
  std::stringstream out;

  ConstraintRenderer::renderSection("type-constraints", records, out);

  const std::string text = out.str();
  REQUIRE(text.find("[type-constraints]") != std::string::npos);
  REQUIRE(text.find("[type] 1:2 x = Int") != std::string::npos);
}

TEST_CASE("ConstraintRenderer: source span formatting", "[ConstraintRenderer]") {
  REQUIRE(ConstraintRenderer::formatSpan(4, 9) == "4:9");
  REQUIRE(ConstraintRenderer::formatSpan(0, 9) == "-");
  REQUIRE(ConstraintRenderer::formatSpan(4, 0) == "-");
}

TEST_CASE("ConstraintRenderer: deterministic ordering", "[ConstraintRenderer]") {
  std::vector<ConstraintRecord> records{
      {"type", 5, 1, "late"},
      {"type", 1, 4, "middle"},
      {"type", 1, 2, "first"},
  };
  std::stringstream out;

  ConstraintRenderer::renderSection("ordering", records, out);
  const std::string text = out.str();

  const auto i_first = text.find("[type] 1:2 first");
  const auto i_middle = text.find("[type] 1:4 middle");
  const auto i_late = text.find("[type] 5:1 late");

  REQUIRE(i_first != std::string::npos);
  REQUIRE(i_middle != std::string::npos);
  REQUIRE(i_late != std::string::npos);
  REQUIRE(i_first < i_middle);
  REQUIRE(i_middle < i_late);
}

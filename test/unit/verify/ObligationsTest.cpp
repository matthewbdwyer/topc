#include "Obligations.h"

#include <catch2/catch_test_macros.hpp>

#include <map>

using namespace verutop;

namespace {
Obligation ob(ObKind k, std::string subject = "", std::string callee = "") {
  Obligation o;
  o.kind = k;
  o.subject = std::move(subject);
  o.callee = std::move(callee);
  return o;
}
} // namespace

// The wording is what the demos, the deck and the test suite quote.
TEST_CASE("each kind of obligation has its message", "[verify][Obligations]") {
  REQUIRE(ob(ObKind::Postcondition).what() == "postcondition");
  REQUIRE(ob(ObKind::Precondition, "mult(x, 2)", "mult").what() == "precondition of mult(x, 2)");
  REQUIRE(ob(ObKind::Assertion).what() == "assertion");
  REQUIRE(ob(ObKind::InvariantOnEntry).what() == "loop invariant holds on entry");
  REQUIRE(ob(ObKind::InvariantPreserved).what() == "loop invariant is preserved");
  REQUIRE(ob(ObKind::LoopMeasure).what() == "loop measure decreases");
  REQUIRE(ob(ObKind::CallMeasure).what() == "recursive call decreases the measure");
  REQUIRE(ob(ObKind::MutualCallMeasure).what() == "mutually recursive call decreases the measure");
  REQUIRE(ob(ObKind::DivisorNotZero).what() == "divisor is not zero");
  REQUIRE(ob(ObKind::NoOverflow, "x + 1").what() == "x + 1 does not overflow");
  REQUIRE(ob(ObKind::ErrorUnreachable).what() == "error statement is unreachable");
  REQUIRE(ob(ObKind::BorrowsDisjoint).what() == "borrowed arguments do not overlap");
  REQUIRE(ob(ObKind::MissingLoopMeasure).what() == "loop needs a decreases clause");
  REQUIRE(ob(ObKind::MissingCallMeasure).what() == "recursive call needs a decreases clause");
  REQUIRE(ob(ObKind::MissingMutualCallMeasure, "f and g").what() ==
          "mutually recursive call needs a decreases clause on f and g");
  REQUIRE(ob(ObKind::LoopMayNotTerminate, "main").what() == "loop may not terminate; main needs decreases *");
  REQUIRE(ob(ObKind::CallMayNotTerminate, "lemma1", "spin").what() ==
          "spin may not terminate; lemma1 needs decreases *");
}

TEST_CASE("a missing clause is never asked of Z3 and never assumed", "[verify][Obligations]") {
  std::map<ObKind, bool> missing{
      {ObKind::Postcondition, false},      {ObKind::Precondition, false},
      {ObKind::Assertion, false},          {ObKind::InvariantOnEntry, false},
      {ObKind::InvariantPreserved, false}, {ObKind::LoopMeasure, false},
      {ObKind::CallMeasure, false},        {ObKind::MutualCallMeasure, false},
      {ObKind::DivisorNotZero, false},     {ObKind::NoOverflow, false},
      {ObKind::ErrorUnreachable, false},   {ObKind::BorrowsDisjoint, false},
      {ObKind::MissingLoopMeasure, true},  {ObKind::MissingCallMeasure, true},
      {ObKind::MissingMutualCallMeasure, true},
      {ObKind::LoopMayNotTerminate, true}, {ObKind::CallMayNotTerminate, true}};
  for (auto &[k, m] : missing) REQUIRE(ob(k).missing() == m);
}

TEST_CASE("an obligation that claims `false` is exempt from the vacuity check", "[verify][Obligations]") {
  // Proving that nothing reaches an error statement is the point.
  REQUIRE(ob(ObKind::ErrorUnreachable).unreachable());
  REQUIRE(ob(ObKind::BorrowsDisjoint).unreachable());
  REQUIRE(ob(ObKind::MissingLoopMeasure).unreachable());
  REQUIRE_FALSE(ob(ObKind::Postcondition).unreachable());
  REQUIRE_FALSE(ob(ObKind::Assertion).unreachable());
  REQUIRE_FALSE(ob(ObKind::Precondition).unreachable());
}

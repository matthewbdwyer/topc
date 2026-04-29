// CubicSolverStandaloneTest.cpp — no AST includes
#include "CubicSolverT.h"

#include <catch2/catch_test_macros.hpp>
#include <vector>

// Use int* as both Variable and Element (pointer identity as key).
// Each test declares its own local ints to ensure independent state.

TEST_CASE("ElementofConstraintAddsElement", "[CubicSolver][standalone]") {
  int x_elem, a_var;
  CubicSolverT<int *, int *> solver({&x_elem});
  solver.addElementofConstraint(&x_elem, &a_var);
  auto result = solver.getElements(&a_var);
  REQUIRE(result.size() == 1);
  REQUIRE(result[0] == &x_elem);
}

TEST_CASE("SubseteqConstraintPropagates", "[CubicSolver][standalone]") {
  int x_elem, a_var, b_var;
  CubicSolverT<int *, int *> solver({&x_elem});
  solver.addElementofConstraint(&x_elem, &a_var);
  solver.addSubseteqConstraint(&a_var, &b_var);
  auto result = solver.getElements(&b_var);
  REQUIRE(result.size() == 1);
  REQUIRE(result[0] == &x_elem);
}

TEST_CASE("ConditionalFiresWhenConditionMet", "[CubicSolver][standalone]") {
  int x_elem, y_elem, in_var, from_var, to_var;
  CubicSolverT<int *, int *> solver({&x_elem, &y_elem});
  // x ∈ [[in]] ⇒ [[from]] ⊆ [[to]]
  solver.addConditionalConstraint(&x_elem, &in_var, &from_var, &to_var);
  // trigger the conditional
  solver.addElementofConstraint(&x_elem, &in_var);
  // now [[from]] ⊆ [[to]]; adding y to from propagates to to
  solver.addElementofConstraint(&y_elem, &from_var);
  auto result = solver.getElements(&to_var);
  REQUIRE(result.size() == 1);
  REQUIRE(result[0] == &y_elem);
}

TEST_CASE("ConditionalDoesNotFireBeforeCondition", "[CubicSolver][standalone]") {
  int x_elem, y_elem, in_var, from_var, to_var;
  CubicSolverT<int *, int *> solver({&x_elem, &y_elem});
  // x ∈ [[in]] ⇒ [[from]] ⊆ [[to]] — but never add x to [[in]]
  solver.addConditionalConstraint(&x_elem, &in_var, &from_var, &to_var);
  solver.addElementofConstraint(&y_elem, &from_var);
  auto result = solver.getElements(&to_var);
  REQUIRE(result.empty());
}

TEST_CASE("TransitiveSubsets", "[CubicSolver][standalone]") {
  int x_elem, a_var, b_var, c_var;
  CubicSolverT<int *, int *> solver({&x_elem});
  solver.addSubseteqConstraint(&a_var, &b_var);
  solver.addSubseteqConstraint(&b_var, &c_var);
  solver.addElementofConstraint(&x_elem, &a_var);
  REQUIRE(!solver.getElements(&a_var).empty());
  REQUIRE(!solver.getElements(&b_var).empty());
  REQUIRE(!solver.getElements(&c_var).empty());
}

TEST_CASE("CycleInSubsets", "[CubicSolver][standalone]") {
  int x_elem, a_var, b_var;
  CubicSolverT<int *, int *> solver({&x_elem});
  solver.addSubseteqConstraint(&a_var, &b_var);
  solver.addSubseteqConstraint(&b_var, &a_var);
  solver.addElementofConstraint(&x_elem, &a_var);
  // Both variables are merged into one node; both should contain x
  REQUIRE(!solver.getElements(&a_var).empty());
  REQUIRE(!solver.getElements(&b_var).empty());
}

TEST_CASE("EmptySolver", "[CubicSolver][standalone]") {
  int a_var;
  CubicSolverT<int *, int *> solver({});
  auto result = solver.getElements(&a_var);
  REQUIRE(result.empty());
}

TEST_CASE("MultipleElements", "[CubicSolver][standalone]") {
  int x_elem, y_elem, z_elem, a_var;
  CubicSolverT<int *, int *> solver({&x_elem, &y_elem, &z_elem});
  solver.addElementofConstraint(&x_elem, &a_var);
  solver.addElementofConstraint(&y_elem, &a_var);
  solver.addElementofConstraint(&z_elem, &a_var);
  auto result = solver.getElements(&a_var);
  REQUIRE(result.size() == 3);
}

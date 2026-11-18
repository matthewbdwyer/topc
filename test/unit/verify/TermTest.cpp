#include "Term.h"
#include "TermPrint.h"

#include <catch2/catch_test_macros.hpp>

using namespace verutop;

namespace {
T x = var("x"), y = var("y"), z = var("z");
T plus(T a, T b) { return op("+", {a, b}); }
T lt(T a, T b) { return op("<", {a, b}); }
std::set<std::string> fv(const T &t) {
  std::set<std::string> s;
  freeVars(t, s);
  return s;
}
} // namespace

TEST_CASE("And drops true and unwraps a single conjunct", "[verify][Term]") {
  REQUIRE(isTrue(And({})));
  REQUIRE(isTrue(And({TRUE, TRUE})));
  T p = lt(x, y);
  REQUIRE(And({TRUE, p}) == p);
  REQUIRE(smt(And({lt(x, y), TRUE, lt(y, z)})) == "(and (< |x| |y|) (< |y| |z|))");
}

TEST_CASE("forall with no variables is its body", "[verify][Term]") {
  T body = lt(x, y);
  REQUIRE(forall({}, {}, body) == body);
}

TEST_CASE("subst replaces free variables only", "[verify][Term]") {
  Sub m{{"x", num(7)}};
  REQUIRE(flat(subst(plus(x, y), m)) == "7 + y");
  // let x = y in x + x: the bound x is not replaced, the value is.
  T l = let("x", plus(x, num(1)), plus(x, x));
  REQUIRE(flat(subst(l, m)) == "let x = 7 + 1 in x + x");
  T q = forall({"x"}, {"Int"}, lt(x, y));
  REQUIRE(flat(subst(q, m)) == "forall x :: x < y");
  REQUIRE(flat(subst(q, {{"y", num(3)}})) == "forall x :: x < 3");
}

TEST_CASE("subst returns the same term when nothing changes", "[verify][Term]") {
  T t = plus(x, y);
  REQUIRE(subst(t, {{"z", num(1)}}) == t);
  REQUIRE(subst(t, {}) == t);
}

TEST_CASE("freeVars respects let and forall", "[verify][Term]") {
  REQUIRE(fv(plus(x, y)) == std::set<std::string>{"x", "y"});
  // In let x = E in B, x is bound in B but free in E.
  REQUIRE(fv(let("x", y, plus(x, z))) == std::set<std::string>{"y", "z"});
  REQUIRE(fv(let("x", x, x)) == std::set<std::string>{"x"});
  REQUIRE(fv(forall({"x"}, {"Int"}, lt(x, y))) == std::set<std::string>{"y"});
  // Operators and function names are not variables.
  REQUIRE(fv(app("f", {x})) == std::set<std::string>{"x"});
}

TEST_CASE("simp folds constants and identities", "[verify][Term]") {
  REQUIRE(flat(simp(plus(num(2), num(3)))) == "5");
  REQUIRE(flat(simp(plus(x, num(0)))) == "x");
  REQUIRE(flat(simp(op("*", {num(1), x}))) == "x");
  REQUIRE(flat(simp(op("*", {x, num(0)}))) == "0");
  REQUIRE(isTrue(simp(lt(num(1), num(2)))));
  REQUIRE(isFalse(simp(lt(num(2), num(1)))));
  REQUIRE(isTrue(simp(op("<=", {x, x}))));
  // Division by zero is left alone: it has no value to fold to.
  REQUIRE(flat(simp(op("tdiv", {num(1), num(0)}))) == "1 / 0");
  REQUIRE(flat(simp(op("tdiv", {num(-7), num(2)}))) == "-3");
}

TEST_CASE("simp simplifies connectives", "[verify][Term]") {
  T p = lt(x, y), q = lt(y, z);
  REQUIRE(flat(simp(op("and", {TRUE, p}))) == "x < y");
  REQUIRE(isFalse(simp(op("and", {p, FALSE}))));
  REQUIRE(isTrue(simp(op("or", {p, TRUE}))));
  REQUIRE(flat(simp(op("and", {p, op("and", {q, p})}))) == "x < y && y < z"); // flattened, no repeats
  REQUIRE(flat(simp(Implies(TRUE, p))) == "x < y");
  REQUIRE(isTrue(simp(Implies(FALSE, p))));
  REQUIRE(isTrue(simp(Implies(p, p))));
  REQUIRE(flat(simp(Implies(p, FALSE))) == "!(x < y)");
  REQUIRE(flat(simp(Not(Not(p)))) == "x < y");
  REQUIRE(flat(simp(op("ite", {TRUE, x, y}))) == "x");
}

TEST_CASE("simp expands let and drops unused bound variables", "[verify][Term]") {
  REQUIRE(flat(simp(let("x", num(2), plus(x, num(3))))) == "5");
  T q = forall({"x", "y"}, {"Int", "Int"}, lt(y, num(1)));
  REQUIRE(flat(simp(q)) == "forall y :: y < 1");
  REQUIRE(isTrue(simp(forall({"x"}, {"Int"}, TRUE))));
}

TEST_CASE("simp knows constructors apart", "[verify][Term]") {
  T leaf = ctor("Leaf", {}), node = ctor("Node", {x});
  REQUIRE(isTrue(simp(is("Leaf", leaf))));
  REQUIRE(isFalse(simp(is("Node", leaf))));
  REQUIRE(isFalse(simp(Eq(leaf, node))));
}

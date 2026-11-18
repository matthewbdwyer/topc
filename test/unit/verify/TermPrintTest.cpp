#include "TermPrint.h"

#include <catch2/catch_test_macros.hpp>

using namespace verutop;

namespace {
T x = var("x"), y = var("y"), z = var("z");
T bin(const char *o, T a, T b) { return op(o, {a, b}); }
} // namespace

TEST_CASE("SMT-LIB: names are quoted, negative numbers are applications", "[verify][TermPrint]") {
  REQUIRE(smt(x) == "|x|");
  REQUIRE(smt(var("x@2")) == "|x@2|");
  REQUIRE(smt(num(5)) == "5");
  REQUIRE(smt(num(-5)) == "(- 5)");
  REQUIRE(smt(TRUE) == "true");
  REQUIRE(smt(bin("+", x, num(1))) == "(+ |x| 1)");
  REQUIRE(smt(app("f", {x, y})) == "(|f| |x| |y|)");
  REQUIRE(smt(ctor("Leaf", {})) == "|Leaf|");
  REQUIRE(smt(is("Node", x)) == "((_ is |Node|) |x|)");
  REQUIRE(smt(sel("Node.left", x)) == "(|Node.left| |x|)");
  REQUIRE(smt(let("x", num(1), x)) == "(let ((|x| 1)) |x|)");
  REQUIRE(smt(forall({"x"}, {"|Tree|"}, TRUE)) == "(forall ((|x| |Tree|)) true)");
}

TEST_CASE("infix: precedence decides the parentheses", "[verify][TermPrint]") {
  REQUIRE(flat(bin("*", bin("+", x, y), z)) == "(x + y) * z");
  REQUIRE(flat(bin("+", x, bin("*", y, z))) == "x + y * z");
  // - and / are not associative: the right operand keeps its parentheses.
  REQUIRE(flat(bin("-", x, bin("-", y, z))) == "x - (y - z)");
  REQUIRE(flat(bin("-", bin("-", x, y), z)) == "x - y - z");
  REQUIRE(flat(bin("tdiv", x, bin("tdiv", y, z))) == "x / (y / z)");
  REQUIRE(flat(bin("+", x, bin("+", y, z))) == "x + y + z");
}

TEST_CASE("infix: connectives and comparisons", "[verify][TermPrint]") {
  T p = bin("<", x, y), q = bin("=", y, z);
  REQUIRE(flat(op("and", {p, q})) == "x < y && y == z");
  REQUIRE(flat(op("or", {op("and", {p, q}), p})) == "x < y && y == z || x < y");
  REQUIRE(flat(op("and", {op("or", {p, q}), p})) == "(x < y || y == z) && x < y");
  REQUIRE(flat(Implies(p, Implies(q, p))) == "x < y ==> y == z ==> x < y");
  REQUIRE(flat(Implies(Implies(p, q), p)) == "(x < y ==> y == z) ==> x < y");
  REQUIRE(flat(Not(q)) == "y != z");
  REQUIRE(flat(Not(p)) == "!(x < y)");
  REQUIRE(flat(op("ite", {p, x, y})) == "(x < y ? x : y)");
}

TEST_CASE("infix: datatypes, rank, and selectors", "[verify][TermPrint]") {
  REQUIRE(flat(ctor("Node", {num(1), ctor("Leaf", {})})) == "Node(1, Leaf)");
  REQUIRE(flat(is("Leaf", x)) == "x is Leaf");
  REQUIRE(flat(sel("Node.left", x)) == "x.left");
  REQUIRE(flat(app("rank#Tree", {x})) == "rank(x)");
}

TEST_CASE("pretty breaks a wide formula at its top-level connectives", "[verify][TermPrint]") {
  T p = bin("<", var("alpha"), var("beta")), q = bin("<", var("gamma"), var("delta"));
  REQUIRE(pretty(op("and", {p, q}), 80) == std::vector<std::string>{"alpha < beta && gamma < delta"});
  REQUIRE(pretty(op("and", {p, q}), 24) == std::vector<std::string>{"alpha < beta &&", "gamma < delta"});
  // The antecedent stays whole; the consequent, indented by two, has two
  // columns fewer and breaks.
  T wide = Implies(op("and", {p, q}), op("and", {q, p}));
  REQUIRE(pretty(wide, 30) ==
          std::vector<std::string>{"alpha < beta && gamma < delta ==>", "  gamma < delta &&", "  alpha < beta"});
}

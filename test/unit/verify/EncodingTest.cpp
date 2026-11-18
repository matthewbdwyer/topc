#include "ProgramHelper.h"
#include "Support.h"
#include "TermPrint.h"

#include <catch2/catch_test_macros.hpp>

using namespace verutop;
using vtest::Program;

namespace {
const char *TREES = R"(
type Tree = Leaf | Node(val, left, right);

size(p)
{
  var n;
  n = 0;
  case *p of {
    Leaf -> n = 0;
    Node(v, l, r) -> n = 1;
  }
  return n;
}
main() {
  var t, n;
  t = Node(1, Leaf, Leaf);
  n = size(&t);
  return n;
}
)";

const char *BORROWS = R"(
set(p, v)
{
  *p = v;
  return 0;
}
forward(q, v)
{
  var d;
  d = set(q, v);
  return d;
}
read(p)
{
  return *p;
}
main() {
  var a, b, d;
  a = 1;
  b = 2;
  d = forward(&a, 5);
  d = read(&b);
  if (d > 0) { b = 3; }
  return a;
}
)";
} // namespace

TEST_CASE("int is Int; a sum type is a datatype; a reference has its referent's sort", "[verify][Encoding]") {
  Program p(TREES);
  auto *size = p.fn("size");
  auto *main = p.fn("main");
  // t in main is a Tree value; p in size borrows one: both have the sort of the value.
  REQUIRE(p.enc.sortOf(main->getDeclarations()[0]->getVars()[0]) == "|Tree|");
  REQUIRE(p.enc.isBorrow(size->getFormals()[0]));
  REQUIRE(p.enc.sortOf(main->getDeclarations()[0]->getVars()[1]) == "Int");
  REQUIRE(p.enc.sortOf(size->getFormals()[0]) == "|Tree|");
  REQUIRE(p.enc.resultSort("size") == "Int");
  REQUIRE(Encoding::isDatatype("|Tree|"));
  REQUIRE_FALSE(Encoding::isDatatype("Int"));
}

TEST_CASE("a sum type is declared with a selector per field", "[verify][Encoding]") {
  Program p(TREES);
  REQUIRE(p.enc.decls.find("(declare-datatypes ( (|Tree| 0)) ( ((|Leaf|)(|Node| (|Node.val| Int) (|Node.left| |Tree|) "
                           "(|Node.right| |Tree|)))))") != std::string::npos);
  REQUIRE(p.enc.decls.find("(declare-fun |rank#Tree| (|Tree|) Int)") != std::string::npos);
  auto fields = p.enc.fieldsOf("Node");
  REQUIRE(fields.size() == 3);
  REQUIRE(fields[1] == std::pair<std::string, std::string>{"Node.left", "|Tree|"});
  REQUIRE(p.enc.fieldsOf("Leaf").empty());
  REQUIRE(p.enc.fieldsOf("NoSuchTag").empty());
}

TEST_CASE("the sort of a term follows its shape", "[verify][Encoding]") {
  Program p(TREES);
  auto lookup = [](const std::string &x) { return x == "t" ? std::string("|Tree|") : std::string("Int"); };
  REQUIRE(p.enc.sortOfTerm(var("t"), lookup) == "|Tree|");
  REQUIRE(p.enc.sortOfTerm(sel("Node.left", var("t")), lookup) == "|Tree|");
  REQUIRE(p.enc.sortOfTerm(sel("Node.val", var("t")), lookup) == "Int");
  REQUIRE(p.enc.sortOfTerm(ctor("Leaf", {}), lookup) == "|Tree|");
  REQUIRE(p.enc.sortOfTerm(is("Leaf", var("t")), lookup) == "Bool");
  REQUIRE(p.enc.sortOfTerm(app("size", {var("t")}), lookup) == "Int");
  REQUIRE(p.enc.sortOfTerm(app("unknownFunction", {var("t")}), lookup) == "Int");
  REQUIRE(p.enc.sortOfTerm(op("ite", {TRUE, var("t"), var("t")}), lookup) == "|Tree|");
  REQUIRE(p.enc.sortOfTerm(op("+", {num(1), num(2)}), lookup) == "Int");
  REQUIRE(p.enc.sortOfTerm(op("<", {num(1), num(2)}), lookup) == "Bool");
}

TEST_CASE("rank: 1 plus the ranks of the sum-valued fields, and at least 1", "[verify][Encoding]") {
  Program p(TREES);
  REQUIRE(p.enc.rankAxioms.count("rank#Tree"));
  auto &ax = p.enc.rankAxioms.at("rank#Tree");
  REQUIRE(ax.name() == "rank!Tree");
  REQUIRE(flat(ax.body) == "rank(x) >= 1 && rank(x) == (x is Leaf ? 1 : 1 + rank(x.left) + rank(x.right))");
  // The induction step: if the sum-valued fields have rank >= 1, so does the value.
  auto &step = p.enc.rankChecks.at("rank#Tree");
  REQUIRE_FALSE(step.definitions); // it must not assume the axiom it justifies
  REQUIRE(step.hypotheses.size() == 2);
  REQUIRE(flat(step.hypotheses[0]) == "x is Node ==> rank(x.left) >= 1");
  REQUIRE(flat(step.goal) == "(x is Leaf ? 1 : 1 + rank(x.left) + rank(x.right)) >= 1");
  REQUIRE(flat(Encoding::rank(var("t"), "|Tree|")) == "rank(t)");
}

TEST_CASE("a case arm is a constructor test, and its binders are selectors", "[verify][Encoding]") {
  Program p(TREES);
  auto stmts = p.fn("size")->getStmts();
  ASTCaseStmt *c = nullptr;
  for (auto *s : stmts)
    if (auto *cs = dynamic_cast<ASTCaseStmt *>(s)) c = cs;
  REQUIRE(c != nullptr);
  auto *node = c->getArms()[1];
  std::vector<std::pair<ASTDeclNode *, T>> binds;
  T cond = p.enc.matchCtor(node->getTag(), node->getPatterns(), var("t"), binds);
  REQUIRE(flat(cond) == "t is Node");
  REQUIRE(binds.size() == 3);
  REQUIRE(binds[0].first->getName() == "v");
  REQUIRE(flat(binds[0].second) == "t.val");
  REQUIRE(flat(binds[2].second) == "t.right");
}

TEST_CASE("a borrowed parameter is in-out only if it may be written", "[verify][Encoding]") {
  Program p(BORROWS);
  REQUIRE(p.enc.isBorrow(p.fn("set")->getFormals()[0]));
  REQUIRE_FALSE(p.enc.isBorrow(p.fn("set")->getFormals()[1]));
  // set writes *p.  forward only passes q on to set: written all the same.
  REQUIRE(p.enc.isWritten("set", 0));
  REQUIRE(p.enc.isWritten("forward", 0));
  // read only reads: the caller's variable is left alone, with no frame condition.
  REQUIRE(p.enc.isBorrow(p.fn("read")->getFormals()[0]));
  REQUIRE_FALSE(p.enc.isWritten("read", 0));
  REQUIRE_FALSE(p.enc.isWritten("set", 1));
}

TEST_CASE("what a statement may change: assignment targets and variables lent for writing", "[verify][Encoding]") {
  Program p(BORROWS);
  auto stmts = p.fn("main")->getStmts();
  using Names = std::set<std::string>;
  REQUIRE(p.enc.assigned(stmts[0]) == Names{"a"});
  // d = forward(&a, 5): d is assigned, and a is lent to a written parameter.
  REQUIRE(p.enc.assigned(stmts[2]) == Names{"a", "d"});
  // d = read(&b): b is lent, but not for writing.
  REQUIRE(p.enc.assigned(stmts[3]) == Names{"d"});
  REQUIRE(p.enc.assigned(stmts[4]) == Names{"b"}); // inside the if
  // *p = v in set assigns p: the reference is its referent.
  REQUIRE(p.enc.assigned(p.fn("set")->getStmts()[0]) == Names{"p"});
}

TEST_CASE("a reference is its referent: *p, **p and &x name one variable", "[verify][Encoding]") {
  Program p(BORROWS);
  auto *store = dynamic_cast<ASTAssignStmt *>(p.fn("set")->getStmts()[0]);
  int derefs = 0;
  REQUIRE(Encoding::rootOf(store->getLHS(), &derefs) == "p");
  REQUIRE(derefs == 1);
  auto *call = dynamic_cast<ASTAssignStmt *>(p.fn("main")->getStmts()[2]);
  auto *app = dynamic_cast<ASTFunAppExpr *>(call->getRHS());
  REQUIRE(Encoding::rootOf(app->getActuals()[0], &derefs) == "a"); // &a
  REQUIRE(derefs == 0);
  REQUIRE(Encoding::rootOf(app->getActuals()[1]).empty()); // 5 names no variable
}

TEST_CASE("a machine integer lies between INT_MIN and INT_MAX", "[verify][Encoding]") {
  REQUIRE(smt(Encoding::inRange(var("x"))) == "(and (<= INT_MIN |x|) (<= |x| INT_MAX))");
}

TEST_CASE("function values are not supported", "[verify][Encoding]") {
  Program p(R"(
id(x) { return x; }
main() {
  var f, r;
  f = id;
  r = f(1);
  return r;
}
)");
  REQUIRE_THROWS_AS(p.enc.sortOf(p.fn("main")->getDeclarations()[0]->getVars()[0]), Unsupported);
}

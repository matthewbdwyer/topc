#include "GclHelpers.h"
#include "Passivize.h"
#include "Query.h"
#include "SmtEncode.h"

#include <catch2/catch_test_macros.hpp>

using namespace vtest;

namespace {
// f(x) == body, as a definition.
Axiom define(const std::string &f, T body) { return Axiom{f, {{"x", "Int"}}, app(f, {v("x")}), Eq(app(f, {v("x")}), body)}; }
Query asking(T goal) {
  Query q;
  q.goal = std::move(goal);
  return q;
}
using Names = std::set<std::string>;
} // namespace

TEST_CASE("a query needs the definitions it mentions, and theirs", "[verify][Query]") {
  Background bg;
  bg.axioms["f"] = define("f", app("g", {v("x")}));
  bg.axioms["g"] = define("g", app("h", {v("x")}));
  bg.axioms["h"] = define("h", num(0));
  bg.axioms["unused"] = define("unused", num(1));
  REQUIRE(definitionsNeeded(bg, asking(Eq(app("f", {num(1)}), num(0)))) == Names{"f", "g", "h"});
  REQUIRE(definitionsNeeded(bg, asking(Eq(app("h", {num(1)}), num(0)))) == Names{"h"});
  REQUIRE(definitionsNeeded(bg, asking(TRUE)).empty());
}

TEST_CASE("definitions that mention each other are found once", "[verify][Query]") {
  Background bg;
  bg.axioms["even"] = define("even", app("odd", {v("x")}));
  bg.axioms["odd"] = define("odd", app("even", {v("x")}));
  REQUIRE(definitionsNeeded(bg, asking(app("even", {num(2)}))) == Names{"even", "odd"});
}

TEST_CASE("only an application counts as a mention", "[verify][Query]") {
  Background bg;
  bg.axioms["fact"] = define("fact", num(1));
  bg.axioms["factorial"] = define("factorial", num(1));
  // A variable or a constructor with a function's name is not the function,
  // and one function's name inside another's is not a mention of it.
  REQUIRE(definitionsNeeded(bg, asking(Eq(v("fact"), num(1)))).empty());
  REQUIRE(definitionsNeeded(bg, asking(Eq(ctor("fact", {}), v("y")))).empty());
  REQUIRE(definitionsNeeded(bg, asking(Eq(app("factorial", {num(3)}), num(6)))) == Names{"factorial"});
}

TEST_CASE("a function whose definition is withheld is simply uninterpreted", "[verify][Query]") {
  // bad's termination check failed, so it has no axiom; a query about it
  // gets nothing, and Z3 may give bad any value.
  Background bg;
  bg.axioms["good"] = define("good", num(1));
  REQUIRE(definitionsNeeded(bg, asking(Eq(app("bad", {num(0)}), app("good", {num(0)})))) == Names{"good"});
}

TEST_CASE("a query can refuse all definitions", "[verify][Query]") {
  Background bg;
  bg.axioms["rank#Tree"] = define("rank#Tree", num(1));
  Query q = asking(op(">=", {app("rank#Tree", {v("x")}), num(1)}));
  q.definitions = false; // the induction step for rank must not assume rank's axiom
  REQUIRE(definitionsNeeded(bg, q).empty());
}

TEST_CASE("symbols are found in definitions, hypotheses and wanted values", "[verify][Query]") {
  Background bg;
  for (auto f : {"a", "b", "c", "d"}) bg.axioms[f] = define(f, num(0));
  Query q = asking(app("a", {num(0)}));
  q.defs.push_back({"N!0", app("b", {num(0)})});
  q.hypotheses.push_back(app("c", {num(0)}));
  q.values.push_back(app("d", {num(0)}));
  REQUIRE(definitionsNeeded(bg, q) == Names{"a", "b", "c", "d"});
}

TEST_CASE("an axiom is a triggered quantified equation", "[verify][Query]") {
  Axiom a = define("f", bin("+", v("x"), num(1)));
  REQUIRE(render(a) == "(assert (forall ((|x| Int)) (! (= (|f| |x|) (+ |x| 1)) :pattern ((|f| |x|)))))\n");
  REQUIRE(render(a, true) ==
          "(assert (! (forall ((|x| Int)) (! (= (|f| |x|) (+ |x| 1)) :pattern ((|f| |x|)))) :named def!f))\n");
  // With no parameters there is nothing to quantify.
  Axiom c{"c", {}, app("c", {}), Eq(app("c", {}), num(4))};
  REQUIRE(render(c) == "(assert (= |c| 4))\n");
  Axiom r = define("rank#Tree", num(1));
  REQUIRE(r.name() == "rank!Tree");
}

TEST_CASE("a query's body: declarations, the negated goal, and what is asked", "[verify][Query]") {
  Query q;
  q.consts = {{"x", "Int"}, {"t", "|Tree|"}};
  q.defs = {{"N!0", bin(">", v("x"), num(0))}};
  q.goal = Implies(v("N!0"), bin(">=", v("x"), num(0)));
  q.values = {v("x")};
  REQUIRE(renderBody(q) == "(declare-const |x| Int)\n"
                           "(declare-const |t| |Tree|)\n"
                           "(define-fun |N!0| () Bool (> |x| 0))\n"
                           "(assert (not (=> |N!0| (>= |x| 0))))\n"
                           "(check-sat)\n"
                           "(get-value (|x| ))\n");
}

TEST_CASE("the header carries the limit and exactly the needed definitions", "[verify][Query]") {
  Background bg;
  bg.rlimit = 500;
  bg.axioms["f"] = define("f", num(0));
  bg.axioms["g"] = define("g", num(0));
  Query q = asking(Eq(app("f", {num(1)}), num(0)));
  std::string h = renderHeader(bg, q);
  REQUIRE(h.find("(set-option :rlimit 500)") != std::string::npos);
  REQUIRE(h.find("(|f| |x|)") != std::string::npos);
  REQUIRE(h.find("(|g| |x|)") == std::string::npos);
  REQUIRE(h.find("INT_MIN") == std::string::npos);
  // A query can carry its own limit (the vacuity check does).
  q.rlimit = 5;
  REQUIRE(renderHeader(bg, q).find("(set-option :rlimit 5)") != std::string::npos);
  bg.overflow = true;
  REQUIRE(renderHeader(bg, q).find("INT_MAX") != std::string::npos);
}

TEST_CASE("when the core is wanted, definitions and flags are named", "[verify][Query]") {
  Background bg;
  bg.rlimit = 500;
  bg.axioms["f"] = define("f", num(0));
  Query q = asking(Eq(app("f", {num(1)}), num(0)));
  q.core = true;
  q.flags = {{"A!0", "asm!0"}};
  std::string text = render(bg, q);
  REQUIRE(text.rfind("(set-option :produce-unsat-cores true)\n", 0) == 0);
  REQUIRE(text.find(":named def!f") != std::string::npos);
  REQUIRE(text.find("(declare-const |A!0| Bool)\n(assert (! |A!0| :named asm!0))\n") != std::string::npos);
  REQUIRE(text.rfind("(check-sat)\n(get-unsat-core)\n") == text.size() - 29);
}

TEST_CASE("a dependency query flags every assumption and every other obligation", "[verify][SmtEncode]") {
  Assume pre = gAssume(bin(">", v("x"), num(0)), 1, "requires");
  pre.asmId = 0;
  Assert missing = check(FALSE, 1);
  missing.missing = true;
  Proc p = procOf({"x"}, gSeq({pre, check(bin(">=", v("x"), num(0)), 0), missing, check(bin(">", v("x"), num(-1)), 2)}), 3);
  p.obs[1].kind = ObKind::MissingLoopMeasure;
  p.assumptions.push_back({Assumption::Pre, 1, "the precondition", ""});
  Passive pv = passivize(p);
  Query q = dependencyQuery(p, pv, 2, false);
  REQUIRE(q.core);
  REQUIRE(q.values.empty());
  // A!0 for the assumption; O!0 for the other real obligation; nothing for
  // the target (#2) or for the missing clause (#1), which is never assumed.
  REQUIRE(q.flags == std::vector<std::pair<std::string, std::string>>{{"A!0", "asm!0"}, {"O!0", "ob!0"}});
  // The flags are free Bool constants in the VC.
  std::set<std::string> fv;
  freeVars(q.goal, fv);
  for (auto &[n, d] : q.defs) freeVars(d, fv);
  REQUIRE(fv.count("A!0"));
  REQUIRE(fv.count("O!0"));
}

TEST_CASE("a trace query asks for every version and every branch condition", "[verify][SmtEncode]") {
  T c = bin(">", v("x"), num(0));
  Proc p = procOf({"x"}, gSeq({ifElse(c, gAssign("y", num(1), 2), gAssign("y", num(2), 3)), check(Eq(v("y"), num(1)), 0)}), 1);
  Passive pv = passivize(p);
  Query q = traceQuery(pv, 0);
  std::vector<std::string> asked;
  for (auto &t : q.values) asked.push_back(flat(t));
  REQUIRE(asked == std::vector<std::string>{"x@0", "y@0", "y@1", "y@2", "x@0 > 0", "!(x@0 > 0)"});
}

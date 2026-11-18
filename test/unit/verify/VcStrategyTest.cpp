#include "GclHelpers.h"
#include "Passivize.h"
#include "SmtEncode.h"
#include "VcStrategy.h"

#include <catch2/catch_test_macros.hpp>

using namespace vtest;

namespace {
// Whether obligation `target` holds, by each strategy, on SMALL.
bool byWp(const Proc &p, int target) { return valid(wpFunction(p, target), p.formals); }
bool byPassive(const Proc &p, int target) {
  Passive pv = passivize(p);
  PassiveVC vc = passiveVc(pv.prog, target);
  std::map<std::string, T> defs(vc.defs.begin(), vc.defs.end());
  return valid(vc.ok, pv.versions, defs);
}

// abs: if (x < 0) r := 0 - x else r := x; assert r >= 0 (#0); assert r == x (#1, false for x < 0)
Proc absProc() {
  T x = v("x"), r = v("r");
  return procOf({"x"},
                gSeq({ifElse(bin("<", x, num(0)), gAssign("r", bin("-", num(0), x), 2), gAssign("r", x, 3)),
                      check(bin(">=", r, num(0)), 0, 4), check(Eq(r, x), 1, 5)}),
                2);
}
} // namespace

TEST_CASE("wp follows the rules for each command", "[verify][VcStrategy]") {
  T Q = bin(">", v("x"), num(0));
  REQUIRE(flat(wp(gAssume(v("p"), 1, ""), Q, 0)) == "p ==> x > 0");
  REQUIRE(flat(wp(gAssume(TRUE, 1, ""), Q, 0)) == "x > 0");
  REQUIRE(flat(wp(gAssign("x", num(5), 1), Q, 0)) == "let x = 5 in x > 0");
  Havoc h;
  h.vars = {"x"};
  h.sorts = {"Int"};
  REQUIRE(flat(wp(h, Q, 0)) == "forall x :: x > 0");
  REQUIRE(flat(wp(gChoice(gAssume(v("p"), 1, ""), gAssume(v("q"), 1, ""), 1), Q, 0)) ==
          "(p ==> x > 0) && (q ==> x > 0)");
}

TEST_CASE("the target assertion is checked, every other one is assumed", "[verify][VcStrategy]") {
  T Q = v("Q");
  REQUIRE(flat(wp(check(v("p"), 3), Q, 3)) == "p && Q");
  REQUIRE(flat(wp(check(v("p"), 3), Q, 4)) == "p ==> Q");
  REQUIRE(flat(wp(check(v("p"), 3), Q, -2)) == "p && Q"); // -2: check them all
  // A missing clause fails as its own obligation and is not assumed after.
  Assert m = check(FALSE, 3);
  m.missing = true;
  REQUIRE(flat(wp(m, Q, 4)) == "Q");
  REQUIRE(flat(wp(m, Q, 3)) == "false && Q");
}

TEST_CASE("the passive VC is the weakest precondition of the passive program", "[verify][VcStrategy]") {
  // For a passive command S: wp(S, Q) == ok(S) && (post(S) ==> Q).  With
  // Q = true, the passive VC ok(S) is wp(S, true).  Checked here on each
  // obligation of two programs, by evaluating both formulas for every
  // value of every version.
  T i = v("i"), n = v("n");
  Havoc h;
  h.vars = {"i"};
  h.sorts = {"Int"};
  T c = bin("<", i, n);
  G body = gSeq({guard(c), gAssign("i", bin("+", i, num(1)), 3), check(bin("<=", i, n), 1), gAssume(FALSE, 3, "")});
  Proc loop = procOf({"n"},
                     gSeq({gAssume(bin(">=", n, num(0)), 1, "requires"), gAssign("i", num(0), 2),
                           check(bin("<=", i, n), 0), h, gAssume(bin("<=", i, n), 3, "loop invariant"),
                           gChoice(body, guard(Not(c)), 3), check(Eq(i, n), 2)}),
                     3);
  for (const Proc &p : {absProc(), loop}) {
    Passive pv = passivize(p);
    for (int target = 0; target < (int)p.obs.size(); ++target) {
      INFO("obligation " << target);
      PassiveVC vc = passiveVc(pv.prog, target);
      std::map<std::string, T> defs(vc.defs.begin(), vc.defs.end());
      T byWp = wpPassive(pv.prog, target);
      T same = op("and", {Implies(byWp, vc.ok), Implies(vc.ok, byWp)});
      REQUIRE(valid(same, pv.versions, defs));
    }
  }
}

TEST_CASE("the passive VC names each post and refers to it by name", "[verify][VcStrategy]") {
  Proc p = absProc();
  Passive pv = passivize(p);
  PassiveVC vc = passiveVc(pv.prog, 0);
  REQUIRE(!vc.defs.empty());
  for (size_t k = 0; k < vc.defs.size(); ++k) REQUIRE(vc.defs[k].first.rfind("N!", 0) == 0);
  // The check of #0 is guarded by the post of everything before it, by name.
  REQUIRE(flat(vc.ok) == vc.defs.back().first + " ==> r@2 >= 0");
  // Definitions the VC does not use are dropped: the post of #1 is not needed for #0.
  PassiveVC all = passiveVc(pv.prog, -2);
  REQUIRE(all.defs.size() >= vc.defs.size());
}

TEST_CASE("versioning alone does not make the VC small, writing the rest once does", "[verify][VcStrategy]") {
  // n ifs in a row, each assigning y; one assertion at the end.  The size
  // of the query text, by each strategy.
  auto size = [](const VcStrategy &strategy, int n) {
    std::vector<G> cs;
    for (int k = 0; k < n; ++k)
      cs.push_back(ifElse(bin(">", v("x"), num(k)), gAssign("y", num(1), 1), gAssign("y", num(2), 1)));
    cs.push_back(check(bin(">", v("y"), num(0)), 0));
    Proc p = procOf({"x"}, gSeq(cs), 1);
    Passive pv = passivize(p);
    return renderBody(obligationQuery(strategy.vc(p, pv, 0))).size();
  };
  SubstVc subst;
  PassiveCopyVc copied;
  PassiveVc once;
  // Written once: doubling the program adds about twice as much text.
  REQUIRE(size(once, 16) - size(once, 8) < (size(once, 8) - size(once, 4)) * 5 / 2);
  // Copied under each branch, with or without substitution: every further
  // if doubles the text, so four more multiply it by about sixteen.
  for (const VcStrategy *s : std::vector<const VcStrategy *>{&subst, &copied}) {
    INFO(s->name());
    REQUIRE(size(*s, 12) > 10 * size(*s, 8));
    REQUIRE(size(*s, 8) > 10 * size(*s, 4));
  }
  REQUIRE(size(once, 12) < size(copied, 12) / 100);
}

TEST_CASE("wp and passive VCs agree on every obligation", "[verify][VcStrategy]") {
  Proc p = absProc();
  REQUIRE(byWp(p, 0));
  REQUIRE(byPassive(p, 0));
  REQUIRE_FALSE(byWp(p, 1));
  REQUIRE_FALSE(byPassive(p, 1));
}

TEST_CASE("an earlier obligation is assumed by a later one", "[verify][VcStrategy]") {
  // assert x > 0 (#0); assert x >= 0 (#1): #0 fails, #1 holds given #0.
  Proc p = procOf({"x"}, gSeq({check(bin(">", v("x"), num(0)), 0), check(bin(">=", v("x"), num(0)), 1)}), 2);
  REQUIRE_FALSE(byWp(p, 0));
  REQUIRE_FALSE(byPassive(p, 0));
  REQUIRE(byWp(p, 1));
  REQUIRE(byPassive(p, 1));
}

TEST_CASE("the loop cut: both strategies agree after a havoc", "[verify][VcStrategy]") {
  // assume n >= 0; i := 0; assert i <= n (#0); havoc i; assume i <= n;
  // { assume i < n; i := i + 1; assert i <= n (#1); assume false } [] { assume !(i < n) };
  // assert i == n (#2)
  T i = v("i"), n = v("n");
  Havoc h;
  h.vars = {"i"};
  h.sorts = {"Int"};
  T c = bin("<", i, n);
  G body = gSeq({guard(c), gAssign("i", bin("+", i, num(1)), 3), check(bin("<=", i, n), 1), gAssume(FALSE, 3, "")});
  Proc p = procOf({"n"},
                  gSeq({gAssume(bin(">=", n, num(0)), 1, "requires"), gAssign("i", num(0), 2),
                        check(bin("<=", i, n), 0), h, gAssume(bin("<=", i, n), 3, "loop invariant"),
                        gChoice(body, guard(Not(c)), 3), check(Eq(i, n), 2)}),
                  3);
  for (int k = 0; k < 3; ++k) {
    INFO("obligation " << k);
    REQUIRE(byWp(p, k));
    REQUIRE(byPassive(p, k));
  }
  // With a weaker invariant (nothing assumed after the havoc) the last one fails in both.
  Proc weak = procOf({"n"}, gSeq({h, gChoice(gSeq({guard(c), gAssume(FALSE, 3, "")}), guard(Not(c)), 3), check(Eq(i, n), 0)}), 1);
  REQUIRE_FALSE(byWp(weak, 0));
  REQUIRE_FALSE(byPassive(weak, 0));
}

TEST_CASE("the three strategies answer through one interface", "[verify][VcStrategy]") {
  Proc p = absProc();
  Passive pv = passivize(p);
  SubstVc wpS;
  PassiveCopyVc copyS;
  PassiveVc passiveS;
  std::vector<const VcStrategy *> both{&wpS, &copyS, &passiveS};
  for (auto *s : both)
    for (int target = 0; target < 2; ++target) {
      INFO(s->name() << " #" << target);
      Vc vc = s->vc(p, pv, target);
      std::vector<std::string> names;
      for (auto &[x, sort] : vc.consts) names.push_back(x);
      std::map<std::string, T> defs(vc.defs.begin(), vc.defs.end());
      REQUIRE(valid(vc.formula, names, defs) == (target == 0));
    }
  // wp declares the formals; passive declares every version.  Both report
  // the constants that hold the arguments, for the counterexample.
  REQUIRE(wpS.vc(p, pv, 0).entry == std::vector<std::string>{"x"});
  REQUIRE(passiveS.vc(p, pv, 0).entry == std::vector<std::string>{"x@0"});
  REQUIRE(copyS.vc(p, pv, 0).entry == std::vector<std::string>{"x@0"});
  // Only the last names anything: the other two write the rest of the function in place.
  REQUIRE(wpS.vc(p, pv, 0).defs.empty());
  REQUIRE(copyS.vc(p, pv, 0).defs.empty());
  REQUIRE(!passiveS.vc(p, pv, 0).defs.empty());
  REQUIRE(makeStrategy(VCGen::VcMode::PassiveCopy)->name() == "wp, passive, rest copied");
}

TEST_CASE("flags mark tracked assumptions and the other obligations", "[verify][SmtEncode]") {
  Assume pre = gAssume(v("p"), 1, "requires");
  pre.asmId = 0;
  G prog = gSeq({pre, gAssume(v("q"), 1, "untracked"), check(v("a"), 0), check(v("b"), 1)});
  auto ks = [](const G &g) { return g.as<Seq>()->kids; };
  // What does the proof of #1 use?
  auto f = ks(flagged(prog, 1, false));
  REQUIRE(flat(f[0].as<Assume>()->p) == "A!0 ==> p");
  REQUIRE(flat(f[1].as<Assume>()->p) == "q");
  REQUIRE(flat(f[2].as<Assert>()->p) == "O!0 ==> a");
  REQUIRE(flat(f[3].as<Assert>()->p) == "b");
  // Does anything reach #1?  The target becomes false.
  auto r = ks(flagged(prog, 1, true));
  REQUIRE(isFalse(r[3].as<Assert>()->p));
  REQUIRE(flat(r[2].as<Assert>()->p) == "O!0 ==> a");
}

TEST_CASE("flags reach inside choices, and skip missing clauses", "[verify][SmtEncode]") {
  Assert missing = check(FALSE, 0);
  missing.missing = true;
  G prog = gChoice(gSeq({missing, check(v("a"), 1)}), check(v("b"), 2), 1);
  auto f = flagged(prog, 2, false);
  auto &arms = f.as<Choice>()->kids;
  REQUIRE(isFalse(arms[0].as<Seq>()->kids[0].as<Assert>()->p)); // no flag: it is never assumed
  REQUIRE(flat(arms[0].as<Seq>()->kids[1].as<Assert>()->p) == "O!1 ==> a");
  REQUIRE(flat(arms[1].as<Assert>()->p) == "b");
}

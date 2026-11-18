#include "GclHelpers.h"

#include <catch2/catch_test_macros.hpp>

using namespace vtest;

TEST_CASE("a default G is the empty sequence", "[verify][Gcl]") {
  G g;
  REQUIRE(g.as<Seq>() != nullptr);
  REQUIRE(g.as<Seq>()->kids.empty());
  REQUIRE(text(g).empty());
}

TEST_CASE("gSeq flattens nested sequences and keeps order", "[verify][Gcl]") {
  G inner = gSeq({gAssign("x", num(1), 1), gAssign("y", num(2), 2)});
  G g = gSeq({inner, gSeq({}), gAssign("z", num(3), 3)});
  auto *s = g.as<Seq>();
  REQUIRE(s != nullptr);
  REQUIRE(s->kids.size() == 3);
  for (auto &k : s->kids) REQUIRE(k.as<Assign>() != nullptr);
  REQUIRE(s->kids[2].as<Assign>()->x == "z");
}

TEST_CASE("each kind keeps its own fields", "[verify][Gcl]") {
  G a = gAssume(TRUE, 7, "note", true);
  REQUIRE(a.as<Assume>()->guard);
  REQUIRE(a.as<Assume>()->line == 7);
  REQUIRE(a.as<Assume>()->asmId == -1);
  REQUIRE(a.as<Assert>() == nullptr);
  G c = gChoice(a, gSeq({}), 9);
  REQUIRE(c.as<Choice>()->kids.size() == 2);
  REQUIRE(c.as<Choice>()->line == 9);
}

TEST_CASE("the printer shows commands, comments and choices", "[verify][Gcl]") {
  Havoc h;
  h.vars = {"x", "y"};
  h.sorts = {"Int", "Int"};
  h.note = "an arbitrary iteration";
  G g = gSeq({h, ifElse(bin(">", v("x"), num(0)), gAssign("y", num(1), 3), gAssign("y", num(2), 4)),
              check(bin(">", v("y"), num(0)), 0, 5, "postcondition")});
  std::string want = "havoc x, y;                                   // an arbitrary iteration\n"
                     "{\n"
                     "  assume x > 0;                               // branch\n"
                     "  y := 1;\n"
                     "} [] {\n"
                     "  assume !(x > 0);                            // branch\n"
                     "  y := 2;\n"
                     "}\n"
                     "assert y > 0;                                 // #0 line 5: postcondition\n";
  REQUIRE(text(g) == want);
}

TEST_CASE("match visits exactly the command's kind", "[verify][Gcl]") {
  auto kind = [](const G &g) {
    return match(
        g, [](const Assume &) { return "assume"; }, [](const Assert &) { return "assert"; },
        [](const Assign &) { return "assign"; }, [](const Havoc &) { return "havoc"; },
        [](const Seq &) { return "seq"; }, [](const Choice &) { return "choice"; });
  };
  REQUIRE(kind(gAssume(TRUE, 1, "")) == std::string("assume"));
  REQUIRE(kind(check(TRUE, 0)) == std::string("assert"));
  REQUIRE(kind(gAssign("x", num(0), 1)) == std::string("assign"));
  REQUIRE(kind(Havoc{}) == std::string("havoc"));
  REQUIRE(kind(gSeq({})) == std::string("seq"));
  REQUIRE(kind(gChoice(gSeq({}), gSeq({}), 1)) == std::string("choice"));
}

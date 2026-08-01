#include "ASTHelper.h"
#include "MoveAnalysis.h"
#include "OwnershipClassifier.h"
#include "SemanticAnalysis.h"
#include "SemanticError.h"
#include "SymbolTable.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <sstream>
#include <string>

// ---------------------------------------------------------------------------
// Helper: build AST + run SemanticAnalysis; expect success.
// ---------------------------------------------------------------------------
static void expectAccepted(std::stringstream &program) {
  auto ast = ASTHelper::build_ast(program);
  REQUIRE_NOTHROW(SemanticAnalysis::analyze(ast.get()));
}

// ---------------------------------------------------------------------------
// Helper: build AST + run SemanticAnalysis; expect SemanticError containing
// the given substring.
// ---------------------------------------------------------------------------
static void expectError(std::stringstream &program, const std::string &msgPart) {
  auto ast = ASTHelper::build_ast(program);
  REQUIRE_THROWS_WITH(SemanticAnalysis::analyze(ast.get()),
                      Catch::Matchers::ContainsSubstring(msgPart));
}

// ---------------------------------------------------------------------------
// Tests include a simple sum declaration to keep fixtures consistent with
// existing semantic test style.
// ---------------------------------------------------------------------------

TEST_CASE("MoveAnalysis: moveTransfersOwnership — accepted, p Moved, q Owned",
          "[MoveAnalysis]") {
  // p = alloc 5; q = p;  (moves p to q)
  // output *q;            (ok — q is Owned)
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    main() {
      var p, q;
      p = alloc 5;
      q = p;
      output *q;
      return 0;
    }
  )";
  expectAccepted(program);
}

TEST_CASE("MoveAnalysis: rejectUseAfterMove — *p after p moved",
          "[MoveAnalysis]") {
  // p = alloc 5; q = p;  (p is Moved)
  // output *p;            (use-after-move error)
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    main() {
      var p, q;
      p = alloc 5;
      q = p;
      output *p;
      return 0;
    }
  )";
  expectError(program, "used after move");
}

TEST_CASE("MoveAnalysis: rejectDoubleMove — r = p after p already moved",
          "[MoveAnalysis]") {
  // p = alloc 5; q = p;  (p is Moved)
  // r = p;                (double-move error)
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    main() {
      var p, q, r;
      p = alloc 5;
      q = p;
      r = p;
      return 0;
    }
  )";
  expectError(program, "moved more than once");
}

TEST_CASE("MoveAnalysis: moveInBothBranches — accepted, p moved on both paths",
          "[MoveAnalysis]") {
  // if (c) q = p; else r = p;   both branches move p → OK at join
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    main() {
      var c, p, q, r;
      c = 1;
      p = alloc 5;
      if (c) {
        q = p;
      } else {
        r = p;
      }
      return 0;
    }
  )";
  expectAccepted(program);
}

TEST_CASE("MoveAnalysis: moveInOneBranchReject — if without else moves p",
          "[MoveAnalysis]") {
  // if (c) q = p;   only one path moves p → disagreement at join
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    main() {
      var c, p, q;
      c = 1;
      p = alloc 5;
      if (c) {
        q = p;
      }
      return 0;
    }
  )";
  expectError(program, "ownership state disagreement");
}

TEST_CASE("MoveAnalysis: copyDoesNotMove — x : int, y = x accepted",
          "[MoveAnalysis]") {
  // x is Copy (int); y = x does NOT move x; output x is fine.
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    main() {
      var x, y;
      x = 5;
      y = x;
      output x;
      return 0;
    }
  )";
  expectAccepted(program);
}

TEST_CASE("MoveAnalysis: rejectAssignOverLiveOwn — p assigned twice without move",
          "[MoveAnalysis]") {
  // p = alloc 5; p = alloc 6;   second alloc overwrites live-owned p → error
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    main() {
      var p;
      p = alloc 5;
      p = alloc 6;
      return 0;
    }
  )";
  expectError(program, "assigned while still owned");
}

TEST_CASE("MoveAnalysis: retained move trace records transitions",
          "[MoveAnalysis]") {
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    main() {
      var p, q;
      p = alloc 5;
      q = p;
      return 0;
    }
  )";

  auto ast = ASTHelper::build_ast(program);
  REQUIRE_NOTHROW(SemanticAnalysis::analyze(ast.get()));

  const auto &trace = MoveAnalysis::getLastTrace();
  REQUIRE_FALSE(trace.empty());

  bool sawMoveP = false;
  bool sawOwnQ = false;
  for (const auto &event : trace) {
    if (event.kind == "move" && event.variable == "p") {
      sawMoveP = true;
    }
    if (event.kind == "own" && event.variable == "q") {
      sawOwnQ = true;
    }
  }

  REQUIRE(sawMoveP);
  REQUIRE(sawOwnQ);
}

TEST_CASE("MoveAnalysis: polymorphic identity moves Own actuals",
          "[MoveAnalysis]") {
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    ident(x) {
      return x;
    }
    main() {
      var p, q;
      p = alloc 5;
      q = ident(p);
      output *p;
      return 0;
    }
  )";

  expectError(program, "used after move");
}

TEST_CASE("MoveAnalysis: polymorphic identity records ownership transfer",
          "[MoveAnalysis][ReferenceMode]") {
  std::stringstream program;
  program << R"(
    identity(value) {
      return value;
    }
    main() {
      var first, second;
      first = alloc 42;
      second = identity(first);
      return *second;
    }
  )";

  auto ast = ASTHelper::build_ast(program);
  REQUIRE_NOTHROW(SemanticAnalysis::analyze(ast.get()));

  bool sawOwnFirst = false;
  bool sawMoveFirst = false;
  bool sawOwnSecond = false;
  for (const auto &event : MoveAnalysis::getLastTrace()) {
    sawOwnFirst |= event.kind == "own" && event.variable == "first";
    sawMoveFirst |= event.kind == "move" && event.variable == "first";
    sawOwnSecond |= event.kind == "own" && event.variable == "second";
  }

  REQUIRE(sawOwnFirst);
  REQUIRE(sawMoveFirst);
  REQUIRE(sawOwnSecond);
}

TEST_CASE("MoveAnalysis: polymorphic identity does not move Copy actuals",
          "[MoveAnalysis]") {
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    ident(x) {
      return x;
    }
    main() {
      var x, y;
      x = 9;
      y = ident(x);
      output x;
      return 0;
    }
  )";

  expectAccepted(program);
}

TEST_CASE("MoveAnalysis: borrow helper call does not consume owner",
          "[MoveAnalysis]") {
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    bump(b) {
      *b = *b + 1;
      return 0;
    }
    main() {
      var x, d;
      x = 5;
      d = bump(&x);
      output x;
      return 0;
    }
  )";

  expectAccepted(program);
}

TEST_CASE("MoveAnalysis: fresh-own return via borrow actual remains owning",
          "[MoveAnalysis]") {
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    mk(seed) {
      return alloc 1;
    }
    main() {
      var x, p;
      x = 1;
      p = mk(&x);
      p = mk(&x);
      return 0;
    }
  )";

  expectError(program, "assigned while still owned");
}

TEST_CASE("MoveAnalysis: polymorphic return from alloc actual remains owning",
          "[MoveAnalysis]") {
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    ident(x) {
      return x;
    }
    main() {
      var p;
      p = ident(alloc 1);
      p = ident(alloc 2);
      return 0;
    }
  )";

  expectError(program, "assigned while still owned");
}

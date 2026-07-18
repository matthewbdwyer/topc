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
// All test programs need at least one sum type so that isTopProgram = true
// and alloc/& use TopOwningRef/TopBorrowRef.
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

// ---------------------------------------------------------------------------
// Record move tests
// ---------------------------------------------------------------------------

TEST_CASE("MoveAnalysis: Copy record assignment is not a move",
          "[MoveAnalysis]") {
  // r2 = r1 where r1:{a:int} — r1 is Copy; still usable afterwards.
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    main() {
      var r1, r2, v;
      r1 = {a:1};
      r2 = r1;
      v = r1.a;
      output v;
      return 0;
    }
  )";
  expectAccepted(program);
}

TEST_CASE("MoveAnalysis: Own record assignment is a move",
          "[MoveAnalysis]") {
  // r2 = r1 where r1:{p:⭡int} — r1 is Own; move transfers ownership.
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    main() {
      var r1, r2;
      r1 = {p: alloc 5};
      r2 = r1;
      return 0;
    }
  )";
  expectAccepted(program);
}

TEST_CASE("MoveAnalysis: use after move of Own record rejected",
          "[MoveAnalysis]") {
  // r1 has Own field p and Copy field tag.
  // After r2=r1 (move), accessing r1.tag must be rejected.
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    main() {
      var r1, r2;
      r1 = {p: alloc 5, tag: 1};
      r2 = r1;
      output r1.tag;
      return 0;
    }
  )";
  expectError(program, "used after move");
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

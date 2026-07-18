#include "ASTHelper.h"
#include "BorrowChecker.h"
#include "SemanticAnalysis.h"
#include "SemanticError.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <sstream>
#include <string>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static void expectAccepted(std::stringstream &program) {
  auto ast = ASTHelper::build_ast(program);
  REQUIRE_NOTHROW(SemanticAnalysis::analyze(ast.get()));
}

static void expectError(std::stringstream &program, const std::string &msgPart) {
  auto ast = ASTHelper::build_ast(program);
  REQUIRE_THROWS_WITH(SemanticAnalysis::analyze(ast.get()),
                      Catch::Matchers::ContainsSubstring(msgPart));
}

// ---------------------------------------------------------------------------
// Phase 10 — Borrow/Lifetime Validity tests
//
// All programs that use alloc include "type Flag = On | Off;" so that
// isTopProgram = true and alloc produces TopOwningRef.
// ---------------------------------------------------------------------------

TEST_CASE("BorrowChecker: validBorrowBeforeMove — borrow then move accepted",
          "[BorrowChecker]") {
  // dummy = inspect(&p); q = p;   borrow is dead after inspect returns, move is fine.
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    inspect(x) { return x; }
    main() {
      var p, q, dummy;
      p = alloc 5;
      dummy = inspect(&p);
      q = p;
      return 0;
    }
  )";
  expectAccepted(program);
}

TEST_CASE("BorrowChecker: borrowInFunctionArgOk — direct function arg accepted",
          "[BorrowChecker]") {
  // f(&p) is the canonical legal borrow usage.
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    peek(x) { return x; }
    main() {
      var p, dummy;
      p = alloc 7;
      dummy = peek(&p);
      return 0;
    }
  )";
  expectAccepted(program);
}

TEST_CASE("BorrowChecker: multipleBorrowArgOk — two borrow args in one call",
          "[BorrowChecker]") {
  // cmp(&p, &q) — both borrows are immediate args — accepted.
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    cmp(a, b) { return a; }
    main() {
      var p, q, dummy;
      p = alloc 1;
      q = alloc 2;
      dummy = cmp(&p, &q);
      return 0;
    }
  )";
  expectAccepted(program);
}

TEST_CASE("BorrowChecker: nestedCallBorrowOk — borrow as arg to inner call",
          "[BorrowChecker]") {
  // use(wrap(&p)) — &p is an immediate arg of wrap, which is fine.
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    wrap(x) { return x; }
    use(x)  { return x; }
    main() {
      var p, dummy;
      p = alloc 3;
      dummy = use(wrap(&p));
      return 0;
    }
  )";
  expectAccepted(program);
}

TEST_CASE("BorrowChecker: rejectBorrowStoredInVar — b = &p is an error",
          "[BorrowChecker]") {
  // b = &p stores the borrow — illegal.
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    use(x) { return x; }
    main() {
      var p, b, dummy;
      p = alloc 5;
      b = &p;
      dummy = use(b);
      return 0;
    }
  )";
  expectError(program, "immediate function argument");
}

TEST_CASE("BorrowChecker: rejectBorrowInIfCond — if (&p) is an error",
          "[BorrowChecker]") {
  // Using &p as an if-condition is an illegal borrow position.
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    main() {
      var p, x;
      p = alloc 1;
      if (&p) {
        x = 1;
      } else {
        x = 0;
      }
      return 0;
    }
  )";
  expectError(program, "immediate function argument");
}

TEST_CASE("BorrowChecker: rejectMoveWhileBorrowedStored — b=&p then q=p is error",
          "[BorrowChecker]") {
  // b = &p already causes an error (borrow stored in variable).
  // Confirms that the sequence b = &p; q = p; use(b) is caught at the
  // borrow-stored step.
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    use(x) { return x; }
    main() {
      var p, q, b, dummy;
      p = alloc 5;
      b = &p;
      q = p;
      dummy = use(b);
      return 0;
    }
  )";
  expectError(program, "immediate function argument");
}

// ---------------------------------------------------------------------------
// Record field borrow
// ---------------------------------------------------------------------------

TEST_CASE("BorrowChecker: borrow of record field accepted",
          "[BorrowChecker]") {
  // readField(&r.val) where val:int — legal borrow of Copy field.
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    readField(x) { return x; }
    main() {
      var r, dummy;
      r = {val: 42};
      dummy = readField(&r.val);
      return 0;
    }
  )";
  expectAccepted(program);
}

TEST_CASE("BorrowChecker: retained trace marks approved borrow",
          "[BorrowChecker]") {
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    peek(x) { return x; }
    main() {
      var p, dummy;
      p = alloc 7;
      dummy = peek(&p);
      return 0;
    }
  )";

  auto ast = ASTHelper::build_ast(program);
  REQUIRE_NOTHROW(SemanticAnalysis::analyze(ast.get()));

  const auto &trace = BorrowChecker::getLastTrace();
  REQUIRE(trace.size() == 1);
  REQUIRE(trace[0].approved);
}

TEST_CASE("BorrowChecker: retained trace marks rejected borrow",
          "[BorrowChecker]") {
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    main() {
      var p, b;
      p = alloc 5;
      b = &p;
      return 0;
    }
  )";

  auto ast = ASTHelper::build_ast(program);
  REQUIRE_THROWS_AS(SemanticAnalysis::analyze(ast.get()), SemanticError);

  const auto &trace = BorrowChecker::getLastTrace();
  REQUIRE(trace.size() == 1);
  REQUIRE_FALSE(trace[0].approved);
}

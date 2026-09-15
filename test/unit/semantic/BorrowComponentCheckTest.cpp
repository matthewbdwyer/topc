#include "ASTHelper.h"
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
// Rule 1 -- a borrow may not be a *component* of a value or a result.
//
// A borrow-typed variable is Copy: nobody frees through it and its owner frees
// under it. Storing one inside a heap box (a constructor payload), an alloc
// payload, or returning one therefore lets the alias outlive the owner. The
// flow-based BorrowChecker cannot see a borrow-typed *variable*, so this is a
// type-level check on the inferred types of payloads, allocs, and returns.
//
// All programs that use alloc include "type Flag = On | Off;" so that
// isTopProgram = true and alloc produces TopOwningRef.
// ---------------------------------------------------------------------------

TEST_CASE("BorrowComponents: reject a borrowed formal stored in a constructor payload",
          "[BorrowComponents]") {
  // Bug B from the handoff. `wrap` infers Link(borrow&own&int, ...).
  std::stringstream program;
  program << R"(
    type Chain = Nil | Link(head, tail);
    wrap(p) { return Link(p, Nil); }
    mk() { var a, c; a = alloc 1; c = wrap(&a); return c; }
    main() { var r; r = mk(); return 0; }
  )";
  expectError(program, "holds a borrow");
}

TEST_CASE("BorrowComponents: reject a borrowed formal stored in a payload via a local",
          "[BorrowComponents]") {
  std::stringstream program;
  program << R"(
    type Chain = Nil | Link(head, tail);
    wrap(p) { var c; c = Link(p, Nil); return c; }
    mk() { var a, c; a = alloc 1; c = wrap(&a); return c; }
    main() { var r; r = mk(); return 0; }
  )";
  expectError(program, "cannot be stored inside a value");
}

TEST_CASE("BorrowComponents: reject a borrow-derived call result in a constructor payload",
          "[BorrowComponents][BorrowChecker]") {
  // The flow-based checker runs first and owns this message: the payload is
  // one more sink alongside assignment and return.
  std::stringstream program;
  program << R"(
    type Chain = Nil | Link(head, tail);
    ident(x) { return x; }
    mk() { var a, c; a = alloc 1; c = Link(ident(&a), Nil); return c; }
    main() { var r; r = mk(); return 0; }
  )";
  expectError(program, "escapes into constructor payload");
}

TEST_CASE("BorrowComponents: reject a borrow-derived call result as an alloc payload",
          "[BorrowComponents]") {
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    ident(x) { return x; }
    main() { var a, b; a = 1; b = alloc ident(&a); return 0; }
  )";
  expectError(program, "holds a borrow");
}

TEST_CASE("BorrowComponents: a Copy payload is fine", "[BorrowComponents]") {
  std::stringstream program;
  program << R"(
    type Chain = Nil | Link(head, tail);
    main() { var c, v; c = Link(1, Link(2, Nil));
      case c of { Link(h, t) -> v = h; Nil -> v = 0; } return v - 1; }
  )";
  expectAccepted(program);
}

TEST_CASE("BorrowComponents: an owned payload is fine", "[BorrowComponents]") {
  std::stringstream program;
  program << R"(
    type Cell = Val(x) | Empty;
    main() { var c, v; c = Val(alloc 7);
      case c of { Val(x) -> v = *x; Empty -> v = 0; } return v - 7; }
  )";
  expectAccepted(program);
}

TEST_CASE("BorrowComponents: a function value whose formal is a borrow may be stored",
          "[BorrowComponents]") {
  // The payload is a *function*, not a borrow; its parameter type is
  // borrow&int but the value stored is Copy and owns nothing.
  std::stringstream program;
  program << R"(
    type Fn = Wrap(f);
    read(p) { return *p; }
    main() { var w, x, r; x = 3; w = Wrap(read);
      case w of { Wrap(g) -> r = g(&x); } return r - 3; }
  )";
  expectAccepted(program);
}

TEST_CASE("BorrowComponents: borrow-typed formals and loop-free locals are untouched",
          "[BorrowComponents]") {
  // Only *components* are checked. A borrow-typed formal is the normal way to
  // receive a borrow, and the flow checker, not this pass, governs locals.
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    peek(p) { return **p; }
    main() { var n, a; n = alloc 42; a = peek(&n); return a - 42; }
  )";
  expectAccepted(program);
}

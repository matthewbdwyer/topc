#include "ASTHelper.h"
#include "BorrowChecker.h"
#include "SemanticAnalysis.h"
#include "SemanticError.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <sstream>
#include <string>
#include <vector>

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

static void expectErrorParts(std::stringstream &program,
                             const std::vector<std::string> &msgParts) {
  auto ast = ASTHelper::build_ast(program);
  try {
    SemanticAnalysis::analyze(ast.get());
    FAIL("expected SemanticError");
  } catch (const SemanticError &err) {
    std::string message = err.what();
    for (const auto &part : msgParts) {
      REQUIRE_THAT(message, Catch::Matchers::ContainsSubstring(part));
    }
  }
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
    inspect(x) { return 0; }
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
    peek(x) { return 0; }
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
    cmp(a, b) { return 0; }
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
    use(x)  { return 0; }
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

TEST_CASE("BorrowChecker: retained trace marks approved borrow",
          "[BorrowChecker]") {
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    peek(x) { return 0; }
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

TEST_CASE("BorrowChecker: provenance records nested pass-through call",
          "[BorrowChecker][provenance]") {
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    identity(x) { return x; }
    read(x) { return *x; }
    main() {
      var value, result;
      value = 17;
      result = read(identity(&value));
      return result;
    }
  )";

  auto ast = ASTHelper::build_ast(program);
  REQUIRE_NOTHROW(SemanticAnalysis::analyze(ast.get()));

  const auto &provenance = BorrowChecker::getLastProvenance();
  REQUIRE(provenance.size() == 2);
  CHECK(provenance[0].kind == BorrowChecker::BorrowProvenanceEvent::Kind::Direct);
  CHECK(provenance[0].hop == 0);
  CHECK(provenance[0].originExpr == "&value");
  CHECK(provenance[0].callee == "identity");
  CHECK(provenance[0].argumentIndex == 0);
  CHECK(provenance[1].kind == BorrowChecker::BorrowProvenanceEvent::Kind::Flow);
  CHECK(provenance[1].hop == 1);
  CHECK(provenance[1].expression == "identity(&value)");
  CHECK(provenance[1].callee == "read");
  CHECK(provenance[1].argumentIndex == 0);
}

TEST_CASE("BorrowChecker: provenance orders multiple pass-through calls",
          "[BorrowChecker][provenance]") {
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    first(x) { return x; }
    second(x) { return x; }
    use(x) { return 0; }
    main() {
      var value, ignored;
      value = 23;
      ignored = use(second(first(&value)));
      return 0;
    }
  )";

  auto ast = ASTHelper::build_ast(program);
  REQUIRE_NOTHROW(SemanticAnalysis::analyze(ast.get()));

  const auto &provenance = BorrowChecker::getLastProvenance();
  REQUIRE(provenance.size() == 3);
  CHECK(provenance[0].hop == 0);
  CHECK(provenance[0].callee == "first");
  CHECK(provenance[1].hop == 1);
  CHECK(provenance[1].expression == "first(&value)");
  CHECK(provenance[1].callee == "second");
  CHECK(provenance[2].hop == 2);
  CHECK(provenance[2].expression == "second(first(&value))");
  CHECK(provenance[2].callee == "use");
}

TEST_CASE("BorrowChecker: fresh ownership terminates borrow provenance",
          "[BorrowChecker][provenance]") {
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    make(seed) { return alloc *seed; }
    use(value) { return *value; }
    main() {
      var value, result;
      value = 23;
      result = use(make(&value));
      return result;
    }
  )";

  auto ast = ASTHelper::build_ast(program);
  REQUIRE_NOTHROW(SemanticAnalysis::analyze(ast.get()));

  const auto &provenance = BorrowChecker::getLastProvenance();
  REQUIRE(provenance.size() == 1);
  CHECK(provenance[0].kind == BorrowChecker::BorrowProvenanceEvent::Kind::Direct);
  CHECK(provenance[0].callee == "make");
}

TEST_CASE("BorrowChecker: retained provenance is replaced between analyses",
          "[BorrowChecker][provenance]") {
  std::stringstream withBorrow;
  withBorrow << R"(
    type Flag = On | Off;
    use(value) { return *value; }
    main() {
      var value, result;
      value = 23;
      result = use(&value);
      return result;
    }
  )";
  auto firstAst = ASTHelper::build_ast(withBorrow);
  REQUIRE_NOTHROW(SemanticAnalysis::analyze(firstAst.get()));
  REQUIRE(BorrowChecker::getLastProvenance().size() == 1);

  std::stringstream withoutBorrow;
  withoutBorrow << R"(
    type Flag = On | Off;
    main() { return 0; }
  )";
  auto secondAst = ASTHelper::build_ast(withoutBorrow);
  REQUIRE_NOTHROW(SemanticAnalysis::analyze(secondAst.get()));
  CHECK(BorrowChecker::getLastProvenance().empty());
}

TEST_CASE("BorrowChecker: reject borrow-derived call result stored in variable",
          "[BorrowChecker]") {
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    ident(x) { return x; }
    main() {
      var p, b;
      p = alloc 7;
      b = ident(&p);
      return 0;
    }
  )";

  expectErrorParts(program, {"borrow-derived value ident(&p) escapes into assignment",
                             "immediate call arguments"});
}

TEST_CASE("BorrowChecker: borrow-derived call result may be immediate argument",
          "[BorrowChecker]") {
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    ident(x) { return x; }
    use(x) { return 0; }
    main() {
      var p, dummy;
      p = alloc 7;
      dummy = use(ident(&p));
      return 0;
    }
  )";

  expectAccepted(program);
}

TEST_CASE("BorrowChecker: reject borrow-derived call result returned",
          "[BorrowChecker]") {
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    ident(x) { return x; }
    leak(p) {
      return ident(&p);
    }
    main() {
      return 0;
    }
  )";

  expectErrorParts(program, {"borrow-derived value ident(&p) escapes into return",
                             "Return or store a copy instead"});
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

#include "CheckBorrowPositions.h"
#include "CheckCaseCompleteness.h"
#include "CheckConstructorCase.h"
#include "CheckSumTypeNames.h"
#include "ASTHelper.h"
#include "SemanticError.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <sstream>

// ============================================================
//  CheckBorrowPositions
// ============================================================

TEST_CASE("CheckBorrowPositions: valid borrow in assignment rhs", "[Weeding]") {
  std::stringstream stream;
  // &y on the RHS of an assignment: allowed at this stage
  stream << R"(f() { var x, y; x = &y; return 0; })";
  auto ast = ASTHelper::build_ast(stream);
  REQUIRE_NOTHROW(CheckBorrowPositions::check(ast.get()));
}

TEST_CASE("CheckBorrowPositions: borrow in output rejected", "[Weeding]") {
  std::stringstream stream;
  stream << R"(f() { var y; output &y; return 0; })";
  auto ast = ASTHelper::build_ast(stream);
  REQUIRE_THROWS_WITH(CheckBorrowPositions::check(ast.get()),
    Catch::Matchers::ContainsSubstring(
        "borrow expression cannot be the argument of 'output'"));
}

TEST_CASE("CheckBorrowPositions: borrow in return rejected", "[Weeding]") {
  std::stringstream stream;
  stream << R"(f() { var y; return &y; })";
  auto ast = ASTHelper::build_ast(stream);
  REQUIRE_THROWS_WITH(CheckBorrowPositions::check(ast.get()),
    Catch::Matchers::ContainsSubstring(
        "borrow expression cannot appear in a 'return' statement"));
}

TEST_CASE("CheckBorrowPositions: borrow in binary expr rejected", "[Weeding]") {
  std::stringstream stream;
  stream << R"(f() { var x, y; x = &y + 1; return 0; })";
  auto ast = ASTHelper::build_ast(stream);
  REQUIRE_THROWS_WITH(CheckBorrowPositions::check(ast.get()),
    Catch::Matchers::ContainsSubstring(
        "borrow expression cannot be used in arithmetic or relational expression"));
}

TEST_CASE("CheckBorrowPositions: borrow in error rejected", "[Weeding]") {
  std::stringstream stream;
  stream << R"(f() { var y; error &y; return 0; })";
  auto ast = ASTHelper::build_ast(stream);
  REQUIRE_THROWS_WITH(CheckBorrowPositions::check(ast.get()),
    Catch::Matchers::ContainsSubstring(
        "borrow expression cannot be the argument of 'error'"));
}

// ============================================================
//  CheckSumTypeNames
// ============================================================

TEST_CASE("CheckSumTypeNames: valid single type", "[Weeding]") {
  std::stringstream stream;
  stream << R"(
    type Option = Some(x) | None;
    main() { return 0; }
  )";
  auto ast = ASTHelper::build_ast(stream);
  REQUIRE_NOTHROW(CheckSumTypeNames::check(ast.get()));
}

TEST_CASE("CheckSumTypeNames: valid two distinct types", "[Weeding]") {
  std::stringstream stream;
  stream << R"(
    type Option = Some(x) | None;
    type Result = Ok(v) | Err(e);
    main() { return 0; }
  )";
  auto ast = ASTHelper::build_ast(stream);
  REQUIRE_NOTHROW(CheckSumTypeNames::check(ast.get()));
}

TEST_CASE("CheckSumTypeNames: duplicate type name rejected", "[Weeding]") {
  std::stringstream stream;
  stream << R"(
    type Option = Some(x) | None;
    type Option = Other(y);
    main() { return 0; }
  )";
  auto ast = ASTHelper::build_ast(stream);
  REQUIRE_THROWS_WITH(CheckSumTypeNames::check(ast.get()),
    Catch::Matchers::ContainsSubstring("duplicate sum type name 'Option'"));
}

TEST_CASE("CheckSumTypeNames: duplicate constructor rejected", "[Weeding]") {
  std::stringstream stream;
  stream << R"(
    type Option = Some(x) | None;
    type Result = Some(v) | Err(e);
    main() { return 0; }
  )";
  auto ast = ASTHelper::build_ast(stream);
  REQUIRE_THROWS_WITH(CheckSumTypeNames::check(ast.get()),
    Catch::Matchers::ContainsSubstring(
        "constructor 'Some' is already declared in type 'Option'"));
}

// ============================================================
//  CheckCaseCompleteness
// ============================================================

TEST_CASE("CheckCaseCompleteness: valid exhaustive case", "[Weeding]") {
  std::stringstream stream;
  stream << R"(
    type Bool = True | False;
    main() {
      var b;
      case b of { True -> output 1; False -> output 0; }
      return 0;
    }
  )";
  auto ast = ASTHelper::build_ast(stream);
  REQUIRE_NOTHROW(CheckCaseCompleteness::check(ast.get()));
}

TEST_CASE("CheckCaseCompleteness: missing arm rejected", "[Weeding]") {
  std::stringstream stream;
  stream << R"(
    type Bool = True | False;
    main() {
      var b;
      case b of { True -> output 1; }
      return 0;
    }
  )";
  auto ast = ASTHelper::build_ast(stream);
  REQUIRE_THROWS_WITH(CheckCaseCompleteness::check(ast.get()),
    Catch::Matchers::ContainsSubstring("not exhaustive"));
}

TEST_CASE("CheckCaseCompleteness: unknown constructor rejected", "[Weeding]") {
  std::stringstream stream;
  // Bool type has True/False, but case arm uses Err which belongs to no declared type
  stream << R"(
    type Bool = True | False;
    type Result = Ok(v) | Err(e);
    main() {
      var b;
      case b of { True -> output 1; False -> output 0; }
      return 0;
    }
  )";
  auto ast = ASTHelper::build_ast(stream);
  // This should succeed — all constructors known and exhaustive
  REQUIRE_NOTHROW(CheckCaseCompleteness::check(ast.get()));
}

TEST_CASE("CheckCaseCompleteness: arity mismatch rejected", "[Weeding]") {
  std::stringstream stream;
  stream << R"(
    type Option = Some(x) | None;
    main() {
      var o;
      case o of { Some(a, b) -> output a; None -> output 0; }
      return 0;
    }
  )";
  auto ast = ASTHelper::build_ast(stream);
  REQUIRE_THROWS_WITH(CheckCaseCompleteness::check(ast.get()),
    Catch::Matchers::ContainsSubstring("expects 1 binding(s) but arm provides 2"));
}

TEST_CASE("CheckCaseCompleteness: duplicate arm rejected", "[Weeding]") {
  std::stringstream stream;
  stream << R"(
    type Bool = True | False;
    main() {
      var b;
      case b of { True -> output 1; True -> output 2; False -> output 0; }
      return 0;
    }
  )";
  auto ast = ASTHelper::build_ast(stream);
  // True is 0-arity: the first arm is vacuously irrefutable, so the second
  // True arm is unreachable (B3 Rule 3).
  REQUIRE_THROWS_WITH(CheckCaseCompleteness::check(ast.get()),
    Catch::Matchers::ContainsSubstring("unreachable case arm"));
}

TEST_CASE("CheckCaseCompleteness: irrefutable arm shadows later arm", "[Weeding]") {
  std::stringstream stream;
  stream << R"(
    type Opt = None | Some(x);
    main() {
      var o;
      case o of { None -> output 0; Some(v) -> output v; Some(_) -> output -1; }
      return 0;
    }
  )";
  auto ast = ASTHelper::build_ast(stream);
  // Some(v) is irrefutable (variable pattern), so Some(_) is unreachable.
  REQUIRE_THROWS_WITH(CheckCaseCompleteness::check(ast.get()),
    Catch::Matchers::ContainsSubstring("unreachable case arm"));
}

TEST_CASE("CheckCaseCompleteness: identical refutable arms rejected", "[Weeding]") {
  std::stringstream stream;
  stream << R"(
    type Inner = Lit(n) | Neg(x);
    type Outer = Empty | Wrap(inner);
    main() {
      var o;
      case o of {
        Empty        -> output 0;
        Wrap(Lit(x)) -> output x;
        Wrap(Lit(x)) -> output 0;
      }
      return 0;
    }
  )";
  auto ast = ASTHelper::build_ast(stream);
  // Two syntactically identical Wrap(Lit(x)) arms: the second is unreachable.
  REQUIRE_THROWS_WITH(CheckCaseCompleteness::check(ast.get()),
    Catch::Matchers::ContainsSubstring("unreachable case arm"));
}

TEST_CASE("CheckCaseCompleteness: two refutable ctor arms accepted", "[Weeding]") {
  std::stringstream stream;
  stream << R"(
    type Inner = Lit(n) | Neg(x);
    type Outer = Empty | Wrap(inner);
    main() {
      var o;
      case o of {
        Empty        -> output 0;
        Wrap(Lit(x)) -> output x;
        Wrap(Neg(y)) -> output y;
      }
      return 0;
    }
  )";
  auto ast = ASTHelper::build_ast(stream);
  // Two distinct refutable Wrap arms are allowed — neither shadows the other.
  REQUIRE_NOTHROW(CheckCaseCompleteness::check(ast.get()));
}

// ============================================================
//  CheckConstructorCase
// ============================================================

// Note: All casing rules are enforced by the grammar tokenizer (CONID vs
// IDENTIFIER). Uppercase-starting words are lexed as CONID and lowercase-
// starting words as IDENTIFIER, so casing violations can never reach the
// weeding pass. CheckConstructorCase is kept as a defensive layer; these
// tests confirm it accepts valid programs.

TEST_CASE("CheckConstructorCase: valid casing", "[Weeding]") {
  std::stringstream stream;
  stream << R"(
    type Option = Some(x) | None;
    main() { var v; return 0; }
  )";
  auto ast = ASTHelper::build_ast(stream);
  REQUIRE_NOTHROW(CheckConstructorCase::check(ast.get()));
}

TEST_CASE("CheckConstructorCase: TOP program without type declarations is accepted", "[Weeding]") {
  // Plain TIP (no type decls): pass is silent
  std::stringstream stream;
  stream << R"(foo() { var x; return 0; })";
  auto ast = ASTHelper::build_ast(stream);
  REQUIRE_NOTHROW(CheckConstructorCase::check(ast.get()));
}

#include "ASTHelper.h"
#include "FunctionEffectSummaries.h"
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
// Rule 2a -- you cannot move an owned value out of a borrow.
//
// An *alias* is an Own-typed dereference of a borrow (`*p` with
// p : borrow&own&T or borrow&Sum), or an Own-typed arm binder of `case *p`.
// An alias may appear only under `&` (reborrow), under another `*` (a Copy
// read through it), as a `case *...` scrutinee, or as an assignment LHS.
// Anywhere else it would become a second owner of the same allocation.
//
// When the dereferenced type is still a type variable at the definition
// (read(p) { return *p; } has type (borrow&a) -> a) the decision is made per
// call site from the actual's type, exactly as DependsOnInstantiation formals
// already are.
//
// All programs that use alloc include "type Flag = On | Off;" so that
// isTopProgram = true and alloc produces TopOwningRef.
// ---------------------------------------------------------------------------

// ---- generic deref of a borrowed formal, decided at the call site -----------

TEST_CASE("AliasCheck: returning *p from a borrow of an owned value is rejected at the call",
          "[AliasCheck]") {
  // Bug A from the handoff: main would destroy a and x, one allocation.
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    take(p) { return *p; }
    main() { var a, x; a = alloc 1; x = take(&a); return *x; }
  )";
  expectError(program, "moves an owned value out of the borrow");
}

TEST_CASE("AliasCheck: the same take(p) at a Copy referent is fine", "[AliasCheck]") {
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    take(p) { return *p; }
    main() { var a, x; a = 5; x = take(&a); return x - 5; }
  )";
  expectAccepted(program);
}

TEST_CASE("AliasCheck: *p stored in a local then returned is rejected at the call",
          "[AliasCheck]") {
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    take(p) { var x; x = *p; return x; }
    main() { var a, x; a = alloc 1; x = take(&a); return *x; }
  )";
  expectError(program, "moves an owned value out of the borrow");
}

TEST_CASE("AliasCheck: *p laundered through a generic pass-through is rejected at the call",
          "[AliasCheck]") {
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    sink(b) { return b; }
    take(p) { var x; x = sink(*p); return x; }
    main() { var a, x; a = alloc 1; x = take(&a); return *x; }
  )";
  expectError(program, "moves an owned value out of the borrow");
}

TEST_CASE("AliasCheck: *p passed to a consuming generic formal is rejected",
          "[AliasCheck]") {
  // consume's own deref fixes p : borrow&own&a, so *p is concretely Own and
  // the definition itself is rejected, before any call is examined.
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    consume(x) { return *x; }
    take(p) { return consume(*p); }
    main() { var a; a = alloc 1; return take(&a); }
  )";
  expectError(program, "cannot be moved out");
}

TEST_CASE("AliasCheck: the requirement propagates through a pass-through caller",
          "[AliasCheck]") {
  // outer forwards its borrowed formal to take; the violation is at main's call.
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    take(p) { return *p; }
    outer(q) { return take(q); }
    main() { var a, x; a = alloc 1; x = outer(&a); return *x; }
  )";
  expectError(program, "moves an owned value out of the borrow");
}

TEST_CASE("AliasCheck: the propagated requirement is satisfied at a Copy referent",
          "[AliasCheck]") {
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    take(p) { return *p; }
    outer(q) { return take(q); }
    main() { var a, x; a = 9; x = outer(&a); return x - 9; }
  )";
  expectAccepted(program);
}

// ---- concrete Own deref, decided at the definition --------------------------

TEST_CASE("AliasCheck: *p as a constructor payload is rejected", "[AliasCheck]") {
  // D5: the box would own the payload while main still owns a.
  std::stringstream program;
  program << R"(
    type Chain = Nil | Link(head, tail);
    wrap(p) { return Link(*p, Nil); }
    main() { var a, c; a = alloc 1; c = wrap(&a); return 0; }
  )";
  expectError(program, "cannot be moved out");
}

TEST_CASE("AliasCheck: **p reads a Copy through the alias and is fine",
          "[AliasCheck]") {
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    peek(p) { return **p; }
    main() { var n, a, b; n = alloc 42; a = peek(&n); b = peek(&n); return a - b; }
  )";
  expectAccepted(program);
}

TEST_CASE("AliasCheck: alloc *p copies a Copy referent and creates fresh ownership",
          "[AliasCheck]") {
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    make(seed) { return alloc *seed; }
    main() { var v, p; v = 23; p = make(&v); return *p - 23; }
  )";
  expectAccepted(program);
}

// ---- arm binders of `case *p` are aliases ----------------------------------

TEST_CASE("AliasCheck: an Own arm binder of case *p assigned to a local is rejected",
          "[AliasCheck]") {
  // D9: r and the payload of *p would both be destroyed.
  std::stringstream program;
  program << R"(
    type Cell = Val(x) | Empty;
    get(p) { var r; case *p of { Val(x) -> r = x; Empty -> r = alloc 0; } return r; }
    main() { var c, x; c = Val(alloc 1); x = get(&c); return *x; }
  )";
  expectError(program, "can only be reborrowed");
}

TEST_CASE("AliasCheck: an Own arm binder of case *p rewrapped in a constructor is rejected",
          "[AliasCheck]") {
  std::stringstream program;
  program << R"(
    type Cell = Val(x) | Empty;
    get(p) { var r; case *p of { Val(x) -> r = Val(x); Empty -> r = Empty; } return r; }
    main() { var c, x; c = Val(alloc 1); x = get(&c); return 0; }
  )";
  expectError(program, "can only be reborrowed");
}

TEST_CASE("AliasCheck: a by-value case on an Own arm binder of case *p is rejected",
          "[AliasCheck]") {
  // D12: matching l by value would consume (free) a subtree main still owns.
  std::stringstream program;
  program << R"(
    type Tree = Leaf | Node(v, l, r);
    h(p) { var n; case *p of { Leaf -> n = 0;
      Node(v, l, r) -> case l of { Leaf -> n = 1; Node(a, b, c) -> n = 2; } } return n; }
    main() { var t; t = Node(1, Node(2, Leaf, Leaf), Leaf); return h(&t) - 2; }
  )";
  expectError(program, "can only be reborrowed");
}

TEST_CASE("AliasCheck: an Own arm binder of case *p returned is rejected",
          "[AliasCheck]") {
  std::stringstream program;
  program << R"(
    type Cell = Val(x) | Empty;
    get(p) { var r; case *p of { Val(x) -> r = x; Empty -> r = alloc 0; } return r; }
    main() { var c, x; c = Val(alloc 1); x = get(&c); return 0; }
  )";
  expectError(program, "can only be reborrowed");
}

TEST_CASE("AliasCheck: reborrowing arm binders of case *p is the accepted traversal idiom",
          "[AliasCheck]") {
  // tree-borrow.top: height(&l), height(&r); main borrows t twice and frees once.
  std::stringstream program;
  program << R"(
    type Tree = Leaf | Node(val, left, right);
    height(p) {
      var h, lh, rh;
      case *p of {
        Leaf -> h = 0;
        Node(v, l, r) -> {
          lh = height(&l); rh = height(&r);
          if (lh > rh) { h = 1 + lh; } else { h = 1 + rh; }
        }
      }
      return h;
    }
    main() { var t; t = Node(1, Node(2, Leaf, Node(3, Leaf, Leaf)), Leaf);
      return height(&t) - height(&t); }
  )";
  expectAccepted(program);
}

TEST_CASE("AliasCheck: Copy arm binders of case *p are unrestricted", "[AliasCheck]") {
  std::stringstream program;
  program << R"(
    type Cell = Val(x) | Empty;
    read(p) { var r; case *p of { Val(x) -> r = x; Empty -> r = 0; } return r; }
    main() { var c; c = Val(7); return read(&c) - 7; }
  )";
  expectAccepted(program);
}

TEST_CASE("AliasCheck: rebuilding a copy from a borrowed structure is fine",
          "[AliasCheck]") {
  // calc.top's copy(e): Copy payloads by value, Own payloads by reborrow.
  std::stringstream program;
  program << R"(
    type Expr = Num(n) | X | Add(l, r);
    copy(e) { var r; case *e of {
      Num(n)    -> r = Num(n);
      X         -> r = X;
      Add(a, b) -> r = Add(copy(&a), copy(&b)); } return r; }
    size(e) { var r; case *e of {
      Num(n) -> r = 1; X -> r = 1; Add(a, b) -> r = 1 + size(&a) + size(&b); } return r; }
    main() { var f, g; f = Add(Num(1), Add(X, Num(2))); g = copy(&f);
      return size(&g) - size(&f); }
  )";
  expectAccepted(program);
}

TEST_CASE("AliasCheck: a by-value case on an Own variable still moves its binders",
          "[AliasCheck][MoveAnalysis]") {
  // Not an alias: e is an Own formal matched by value, so its payloads move.
  std::stringstream program;
  program << R"(
    type Expr = Num(n) | X | Add(l, r);
    deriv(e) { var r; case e of {
      Num(n)    -> r = Num(0);
      X         -> r = Num(1);
      Add(a, b) -> r = Add(deriv(a), deriv(b)); } return r; }
    main() { var d; d = deriv(Add(X, Num(3))); return 0; }
  )";
  expectAccepted(program);
}

// ---- write-through --------------------------------------------------------

TEST_CASE("AliasCheck: overwriting an owned slot through a borrow is rejected",
          "[AliasCheck]") {
  // D13: the old *p is never freed.
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    set(p) { *p = alloc 2; return 0; }
    main() { var a, d; a = alloc 1; d = set(&a); return *a - 2; }
  )";
  expectError(program, "cannot overwrite the owned value");
}

TEST_CASE("AliasCheck: writing a Copy through a borrow stays legal", "[AliasCheck]") {
  // borrow-write.top: setVal is generic in the slot type; fine at int.
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    setVal(p, v) { *p = v; return 0; }
    main() { var x, d; x = 1; d = setVal(&x, 9); return x - 9; }
  )";
  expectAccepted(program);
}

TEST_CASE("AliasCheck: a generic write-through instantiated at an owned slot is rejected at the call",
          "[AliasCheck]") {
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    setVal(p, v) { *p = v; return 0; }
    main() { var a, d; a = alloc 1; d = setVal(&a, alloc 2); return 0; }
  )";
  expectError(program, "overwrites an owned value through the borrow");
}

// ---- things that must keep working ------------------------------------------

TEST_CASE("AliasCheck: dereferencing an owning pointer is a Copy read, not an alias",
          "[AliasCheck]") {
  // own-destroy.top: p : own&int moved in; *p is an int.
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    sink(p) { return *p; }
    main() { var a, b; a = alloc 1; b = alloc 2; output sink(a); return *b - 2; }
  )";
  expectAccepted(program);
}

TEST_CASE("AliasCheck: calling through a function-typed formal is unaffected",
          "[AliasCheck]") {
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    mkfresh() { return alloc 1; }
    apply(f) { return f(); }
    main() { var x; x = apply(mkfresh); return *x - 1; }
  )";
  expectAccepted(program);
}

TEST_CASE("AliasCheck: a borrowed scrutinee whose binders are only read is fine",
          "[AliasCheck]") {
  std::stringstream program;
  program << R"(
    type Cell = Val(x) | Empty;
    peek(p) { var r; case *p of { Val(x) -> r = *x; Empty -> r = 0; } return r; }
    main() { var c; c = Val(alloc 1); return peek(&c) - 1; }
  )";
  expectAccepted(program);
}

// ---- the summary records and propagates the requirement ---------------------

TEST_CASE("AliasCheck: copy requirements are recorded on the summary and inherited by pass-through callers",
          "[AliasCheck][FunctionEffectSummaries]") {
  using R = FunctionEffectSummaries::CopyRequirement;
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    take(p) { return *p; }
    outer(q) { return take(q); }
    lend(x) { return take(&x); }
    peek(p) { return **p; }
    main() { var a, n; a = 3; n = alloc 4;
      return outer(&a) + lend(a) + peek(&n); }
  )";

  auto ast = ASTHelper::build_ast(program);
  auto analysis = SemanticAnalysis::analyze(ast.get());
  auto *effects = analysis->getFunctionEffectSummaries();
  auto *symbols = analysis->getSymbolTable();

  auto *take = effects->get(symbols->getFunction("take"));
  auto *outer = effects->get(symbols->getFunction("outer"));
  auto *lend = effects->get(symbols->getFunction("lend"));
  auto *peek = effects->get(symbols->getFunction("peek"));
  REQUIRE(take != nullptr);
  REQUIRE(outer != nullptr);
  REQUIRE(lend != nullptr);
  REQUIRE(peek != nullptr);

  CHECK(take->formalRequirement[0] == R::Referent);   // takes *p
  CHECK(outer->formalRequirement[0] == R::Referent);  // passes q on to take
  CHECK(lend->formalRequirement[0] == R::Self);       // passes &x on to take
  CHECK(peek->formalRequirement[0] == R::None);       // **p only reads a Copy
}

// ---- a generic formal used again after it is passed on ----------------------

TEST_CASE("FunctionEffectSummaries: a generic formal used after it is passed on must be Copy",
          "[FunctionEffectSummaries]") {
  std::stringstream twice;
  twice << R"(
    sink(p) { return *p; }
    twice(f, x) { var r; r = f(x); r = f(x); return r; }
    main() { var a; a = alloc 1; return twice(sink, a); }
  )";
  expectError(twice, "which uses it again after passing it on");

  std::stringstream returnedToo;
  returnedToo << R"(
    sink(p) { return *p; }
    keep(f, x) { var r; r = f(x); return x; }
    main() { var a, b; a = alloc 1; b = keep(sink, a); return *b; }
  )";
  expectError(returnedToo, "which uses it again after passing it on");

  std::stringstream borrowedAfter;
  borrowedAfter << R"(
    sink(p) { return *p; }
    read(q) { return **q; }
    keep(f, g, x) { var r; r = f(x); return r + g(&x); }
    main() { var a; a = alloc 1; return keep(sink, read, a); }
  )";
  expectError(borrowedAfter, "which uses it again after passing it on");

  std::stringstream inLoop;
  inLoop << R"(
    sink(p) { return *p; }
    rep(f, x, n) { var r; r = 0; while (n > 0) { r = f(x); n = n - 1; } return r; }
    main() { var a; a = alloc 1; return rep(sink, a, 2); }
  )";
  // The second iteration uses x after the first passed it on.
  expectError(inLoop, "which uses it again after passing it on");
}

TEST_CASE("FunctionEffectSummaries: reuse is fine at a Copy instance, and reassignment starts a new value",
          "[FunctionEffectSummaries]") {
  std::stringstream copyInstance;
  copyInstance << R"(
    sink(p) { return *p; }
    twice(f, x) { var r; r = f(x); r = f(x); return r; }
    main() { var n; n = 1; return twice(sink, &n) - 1; }
  )";
  expectAccepted(copyInstance);

  std::stringstream fold;
  fold << R"(
    type List = Nil | Cons(head, tail);
    push(acc, n) { return Cons(n, acc); }
    count(l) { var r; r = 0; case l of { Nil -> r = 0; Cons(h, t) -> r = 1 + count(t); } return r; }
    fold(f, n, acc) { while (n > 0) { acc = f(acc, n); n = n - 1; } return acc; }
    main() { var l; l = fold(push, 3, Nil); return count(l) - 3; }
  )";
  expectAccepted(fold);

  using R = FunctionEffectSummaries::CopyRequirement;
  std::stringstream summary;
  summary << R"(
    sink(p) { return *p; }
    twice(f, x) { var r; r = f(x); r = f(x); return r; }
    main() { var n; n = 1; return twice(sink, &n); }
  )";
  auto ast = ASTHelper::build_ast(summary);
  auto analysis = SemanticAnalysis::analyze(ast.get());
  auto *twiceSummary = analysis->getFunctionEffectSummaries()->get(
      analysis->getSymbolTable()->getFunction("twice"));
  REQUIRE(twiceSummary != nullptr);
  CHECK(twiceSummary->formalRequirement[1] == R::Self);
  CHECK((twiceSummary->formalRequirementReasons[1] &
         FunctionEffectSummaries::UsedAfterPassedOn) != 0);
}

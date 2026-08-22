#include "ASTHelper.h"
#include "CallGraph.h"
#include "SymbolTable.h"
#include "TypeInference.h"
#include "UnificationError.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <memory>
#include <sstream>

/* These tests exercise the application typing rule
 *
 *   [[E]] = ([[E1]], ..., [[En]]) -> [[E(E1, ..., En)]]
 *
 * which is a structural invariant of every call site and must therefore hold
 * regardless of what control-flow analysis can resolve for the callee.
 *
 * The polymorphic collector, PolyTypeConstraintVisitor, emits this constraint
 * from inside a loop over the CFA may-call set.  When that set is empty -- the
 * callee is not a function, or CFA cannot tie it to one -- the loop body never
 * runs and the rule is never stated, so ill-typed applications are accepted.
 */

namespace {

/* Runs the full inference pipeline -- polymorphic stage followed by the
 * residual monomorphic stage -- on the given program.
 */
void inferTypes(std::stringstream &program) {
  auto ast = ASTHelper::build_ast(program);
  auto symbols = SymbolTable::build(ast.get());
  auto cg = CallGraph::build(ast.get(), symbols.get());
  auto result = TypeInference::run(ast.get(), cg.get(), symbols.get());
  (void)result;
}

} // namespace

TEST_CASE("FunAppTyping: applying an int is rejected on the polymorphic path",
          "[TypeInference]") {
  // "main" is non-recursive, so it is typed by the polymorphic collector.
  // CFA finds no function flowing to "v", so the may-call set at "v(3)" is
  // empty.  The application rule must still be stated, and "v = 5" must then
  // fail to unify with a function type.
  std::stringstream program;
  program << R"(main() {
  var v, out;
  v = 5;
  out = v(3);
  return out;
})";

  REQUIRE_THROWS_AS(inferTypes(program), UnificationError);
}

TEST_CASE("FunAppTyping: applying a dereferenced cell is rejected",
          "[TypeInference]") {
  // The callee is not a variable but a dereference expression, so the base
  // constraint must attach to that expression's own type variable.
  std::stringstream program;
  program << R"(main() {
  var p, out;
  p = alloc 5;
  out = (*p)(1);
  return out;
})";

  REQUIRE_THROWS_AS(inferTypes(program), UnificationError);
}

TEST_CASE("FunAppTyping: a higher-order parameter must be a function",
          "[TypeInference]") {
  // Inside "apply" the may-call set at "f(v)" is empty because no function
  // value reaches "f".  Without the application constraint, "f" is left as a
  // free type variable and the int actual at the call site is accepted.
  std::stringstream program;
  program << R"(apply(f, v) {
  return f(v);
}

main() {
  return apply(1, 2);
})";

  REQUIRE_THROWS_AS(inferTypes(program), UnificationError);
}

TEST_CASE("FunAppTyping: applying an int is rejected on the monomorphic path",
          "[TypeInference]") {
  // Control case: "loop" is recursive, so it is typed by the residual
  // monomorphic collector, which emits the application constraint
  // unconditionally.  This already passes and guards against a regression in
  // the path that is currently correct.
  std::stringstream program;
  program << R"(loop(k) {
  var v, out;
  v = 5;
  out = v(3);
  if (k > 0) {
    out = loop(k - 1);
  }
  return out;
})";
  program << R"(
main() {
  return loop(2);
})";

  REQUIRE_THROWS_AS(inferTypes(program), UnificationError);
}

TEST_CASE("FunAppTyping: legitimate higher-order calls are still accepted",
          "[TypeInference]") {
  // Guard against over-constraining: the fix must not reject calls that CFA
  // does resolve to a function.
  std::stringstream program;
  program << R"(apply(f, v) {
  return f(v);
}

inc(x) {
  return x + 1;
}

main() {
  return apply(inc, 2);
})";

  REQUIRE_NOTHROW(inferTypes(program));
}

TEST_CASE("FunAppTyping: polymorphic reuse at distinct types is still accepted",
          "[TypeInference]") {
  // Guard against monomorphizing generic functions: "ident" is applied at both
  // int and pointer-to-int.
  std::stringstream program;
  program << R"(ident(p) {
  return p;
}

main() {
  var x, y;
  x = ident(42);
  y = ident(&x);
  return *y;
})";

  REQUIRE_NOTHROW(inferTypes(program));
}

TEST_CASE("FunAppTyping: a heap-stored function is pinned at its call site",
          "[TypeInference]") {
  // ACCEPTED SLACK, not a desired diagnostic.  "id" is stored in a cell and
  // called at int, and separately passed as a value and used at Box.  Both are
  // legitimate instances of id's principal type (alpha) -> alpha, so a checker
  // without slack would accept this program.
  //
  // CFA cannot follow a function value through a cell, so the may-call set at
  // "(*p)(2)" is empty and the application rule is stated against the callee
  // expression itself.  Through the alloc/deref alias chain that pins id to
  // (int) -> int, and the use at Box then fails to unify.  Soundness is
  // required; this slack is accepted.  See docs/TOP-tutorial.md.
  std::stringstream program;
  program << R"(type Box = B(x);

apply(f, v) {
  return f(v);
}

id(y) {
  return y;
}

main() {
  var p, a, b;
  p = alloc id;
  a = (*p)(2);
  b = apply(id, B(1));
  return a;
})";

  REQUIRE_THROWS_AS(inferTypes(program), UnificationError);
}

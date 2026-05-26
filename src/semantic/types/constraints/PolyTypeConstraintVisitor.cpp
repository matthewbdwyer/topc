#include <cassert>
#include "PolyTypeConstraintVisitor.h"
#include "ASTVariableExpr.h"
#include "FreshAlphaCopier.h"
#include "TypeVars.h"
#include "TopMu.h"
#include "TopTypeVisitor.h"
#include "loguru.hpp"

/// Returns true if \p t contains any TopMu node at any nesting level.
static bool containsTopMu(TopType *t) {
  struct Finder : public TopTypeVisitor {
    bool found = false;
    bool visit(TopMu *) override { found = true; return false; }
  } finder;
  t->accept(&finder);
  return finder.found;
}

PolyTypeConstraintVisitor::PolyTypeConstraintVisitor(
    SymbolTable *st, CallGraph *cg, std::shared_ptr<Unifier> u,
    std::unique_ptr<ConstraintHandler> handler)
    : TypeConstraintVisitor(st, std::move(handler)), callGraph(cg),
      unifier(u){};

/*! \brief Polymorphic type constraints for function application.
 *
 * Type Rules for "E(E1, ..., En)":
 *  [[E]] = ([[E1]], ..., [[En]]) -> [[E(E1, ..., En)]]
 *
 * Polymorphic typing handles the case where expression "E" can
 * have a value of a function declared with the "poly" keyword.
 * The set of functions that may be called at this call site
 * is computed by CFA and accessed through the call graph.   The "poly"
 * attribute is accessed through the symbol table.
 *
 * For such functions, we generate constraints for this call site by
 * creating a copy of the type for the called function with "fresh"
 * type variables for its arguments.  The type for the called function
 * has already been computed, this is enforced by type analysis, and it
 * is called the "generic" type.   The copy is called the "instantiated"
 * type.  The idea is that for each call site we create distinct instantiated
 * types and this makes it possible for a function to match distinct argument
 * and return value typings, e.g., to be polymorphic.
 *
 * Note that code generation works just fine with polymorphic typing as long
 * as all of the types have a common representation, i.e., they fit into the
 * same number of bits.
 *
 */
void PolyTypeConstraintVisitor::endVisit(ASTFunAppExpr *element) {
  std::vector<std::shared_ptr<TopType>> actuals;
  for (auto &a : element->getActuals()) {
    actuals.push_back(astToVar(a));
  }

  /* For each called function:
   *   - if it is declared polymorphic AND the call is a direct named call
   *     (not a higher-order parameter call), instantiate with fresh type vars
   *   - otherwise use the monomorphic approach
   *
   * The "direct named call" check prevents incorrect poly instantiation inside
   * higher-order functions (e.g., apply(f,v){return f(v);}) where CFA resolves
   * `f` to several possibly-poly functions that would generate conflicting types.
   */
  auto *calleeExpr = dynamic_cast<ASTVariableExpr *>(element->getFunction());
  bool isDirectNamedCall = calleeExpr != nullptr &&
                           symbolTable->getFunction(calleeExpr->getName()) != nullptr;

  for (auto f : callGraph->getCalledFuns(element)) {
    auto fName = f->getName();
    auto fDecl = symbolTable->getFunction(fName);
    auto isPoly = symbolTable->getPoly(fName);

    if (isPoly && isDirectNamedCall) {
      auto genericType = unifier->inferred(astToVar(fDecl));

      // Only do poly instantiation when the closed type has free type
      // variables.  If there are none (e.g. fully-concrete or recursive record
      // types), fresh copying would re-introduce TopMu nodes into the solver,
      // which the TermUnifier cannot handle.  Fall through to mono instead.
      auto freeVars = TypeVars::collect(genericType.get());
      if (!freeVars.empty() && !containsTopMu(genericType.get())) {
        auto copyType = FreshAlphaCopier::copy(genericType.get(), element);

        auto instantiatedType = std::dynamic_pointer_cast<TopFunction>(copyType);
        assert(instantiatedType != nullptr);

        LOG_S(1) << "Polymorphic type constraint for application of " << fName
                 << " with generic type " << *genericType
                 << " using instantiated type " << *instantiatedType;

        // Polymorphic function application
        constraintHandler->handle(
            instantiatedType,
            std::make_shared<TopFunction>(actuals, astToVar(element)));
        continue;
      }
      // else: fall through to monomorphic handling below
    }
    {
      // Monomorphic function application
      constraintHandler->handle(
          astToVar(element->getFunction()),
          std::make_shared<TopFunction>(actuals, astToVar(element)));
    }
  }
}

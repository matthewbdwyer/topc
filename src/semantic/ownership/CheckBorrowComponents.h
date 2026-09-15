#pragma once

#include "ASTVisitor.h"

#include <set>

class ASTAllocExpr;
class ASTProgram;
class SymbolTable;
class TopType;
class TypeInference;

/*! \class CheckBorrowComponents
 *  \brief Reject a borrow that is a *component* of a value or a result.
 *
 * A borrow-typed value is Copy: nobody frees through it and its owner frees
 * under it. That is safe while the borrow lives only in a formal or an
 * immediate call argument, because the callee returns before the owner can
 * move. Stored *inside* another value -- a constructor payload, an `alloc`
 * payload -- or handed back as a function's result, the same borrow outlives
 * the call and dangles as soon as the owner is destroyed.
 *
 * The flow-based BorrowChecker sees `&x` and borrow-returning calls, but it
 * cannot see a borrow-typed *variable* (a formal) being stored. This pass
 * closes that gap at the type level: after inference it walks the solved type
 * of every sum-type payload, every `alloc` payload, and every function's
 * return, and rejects any occurrence of a borrow mode inside them. Formals and
 * locals are not checked; they are where a borrow legitimately lives.
 *
 * Runs after BorrowChecker::checkInterprocedural so that, where both would
 * fire, the flow checker's more specific message wins.
 */
class CheckBorrowComponents : public ASTVisitor {
public:
  static void check(ASTProgram *p, SymbolTable *sym, TypeInference *types);

  /*! \brief True if a borrow mode occurs anywhere in `type`, not looking
   *  inside function types (a stored function value owns nothing). */
  static bool containsBorrow(const TopType *type);

private:
  CheckBorrowComponents(SymbolTable *sym, TypeInference *types)
      : sym(sym), types(types) {}

  void endVisit(ASTAllocExpr *element) override;

  void checkSumTypes();
  void checkFunctionReturns();

  static bool containsBorrow(const TopType *type,
                             std::set<const TopType *> &visited);

  SymbolTable *sym;
  TypeInference *types;
};

#pragma once

#include "ASTVisitor.h"

#include <set>

class ASTProgram;
class ASTBorrowExpr;
class ASTFunAppExpr;

/*! \class BorrowChecker
 *  \brief Phase 10 — Borrow/Lifetime Validity.
 *
 * Enforces the immediate-argument restriction (Q21): a borrow expression
 * (`&x`) is legal **only** as a direct argument of a function call.
 * Storing a borrow in a variable, using it in a condition, or in any
 * other position is a SemanticError.
 *
 * Because borrows are proven by this pass to be call-scoped, no CFG or
 * lifetime region analysis is required: the borrow is dead as soon as the
 * callee returns, so the owner may safely be moved afterwards.
 */
class BorrowChecker : public ASTVisitor {
public:
  static void check(ASTProgram *p);

private:
  BorrowChecker() = default;

  // Set of ASTBorrowExpr nodes that appear as direct arguments of a call.
  std::set<ASTBorrowExpr *> approvedBorrows;

  bool visit(ASTFunAppExpr *element) override;
  void endVisit(ASTBorrowExpr *element) override;
};

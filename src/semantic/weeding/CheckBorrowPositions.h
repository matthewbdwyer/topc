#pragma once

#include "ASTVisitor.h"

/*! \class CheckBorrowPositions
 *  \brief Reject unambiguously invalid borrow-expression positions.
 *
 * A borrow `&x` is legal only as a call argument (a borrow may write a Copy
 * value through it, but never outlives the call). The following positions
 * are rejected early (before type inference):
 *   - operand of an arithmetic or relational binary expression
 *   - argument of `output`
 *   - argument of `error`
 *   - operand of `return`
 *
 * Borrows as function-call arguments and as the RHS of an assignment are
 * deferred to the type/borrow checker.
 */
class CheckBorrowPositions : public ASTVisitor {
public:
  CheckBorrowPositions() = default;
  static void check(ASTProgram *p);
  virtual void endVisit(ASTBinaryExpr *element) override;
  virtual void endVisit(ASTOutputStmt *element) override;
  virtual void endVisit(ASTErrorStmt *element) override;
  virtual void endVisit(ASTReturnStmt *element) override;
};

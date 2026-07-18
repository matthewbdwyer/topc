#pragma once

#include "ASTVisitor.h"

#include <set>
#include <string>
#include <vector>

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
  struct BorrowTraceEvent {
    int line;
    int column;
    std::string expr;
    bool approved;
  };

  static void check(ASTProgram *p);

  /*! \brief Returns retained borrow trace from the most recent run. */
  static const std::vector<BorrowTraceEvent> &getLastTrace();

private:
  BorrowChecker() = default;

  // Set of ASTBorrowExpr nodes that appear as direct arguments of a call.
  std::set<ASTBorrowExpr *> approvedBorrows;
  std::vector<BorrowTraceEvent> trace;
  static std::vector<BorrowTraceEvent> lastTrace;

  bool visit(ASTFunAppExpr *element) override;
  void endVisit(ASTBorrowExpr *element) override;
};

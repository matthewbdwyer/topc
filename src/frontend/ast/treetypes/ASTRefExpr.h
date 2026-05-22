#pragma once

#include "ASTExpr.h"

/*! \brief Class for a borrow expression (&x).
 *
 * In TOP, & always denotes a borrow (read-only reference); TipRef is retired.
 * This class was formerly named ASTRefExpr; ASTRefExpr is kept as a typedef
 * for backward compatibility with downstream passes not yet updated.
 */
class ASTBorrowExpr : public ASTExpr {
  std::shared_ptr<ASTExpr> VAR;

public:
  std::vector<std::shared_ptr<ASTNode>> getChildren() override;
  ASTBorrowExpr(std::shared_ptr<ASTExpr> VAR) : VAR(VAR) {}
  ASTExpr *getVar() const { return VAR.get(); }
  void accept(ASTVisitor *visitor) override;

protected:
  std::ostream &print(std::ostream &out) const override;
};

// Backward compat: code not yet updated to ASTBorrowExpr still compiles.
using ASTRefExpr = ASTBorrowExpr;

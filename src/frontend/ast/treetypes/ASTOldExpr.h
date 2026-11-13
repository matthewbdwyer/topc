#pragma once

#include "ASTExpr.h"
#include "ASTVisitor.h"

/*! \brief VERUTOP: old(E) in an ensures clause, the value of E on entry.
 *
 * It appears only in specifications, so only the passes that walk
 * specifications (symbols and types) ever see it.
 */
class ASTOldExpr : public ASTExpr {
  std::shared_ptr<ASTExpr> EXPR;

public:
  explicit ASTOldExpr(std::shared_ptr<ASTExpr> e) : EXPR(std::move(e)) {}
  ASTExpr *getExpr() const { return EXPR.get(); }
  std::vector<std::shared_ptr<ASTNode>> getChildren() override { return {EXPR}; }

  void accept(ASTVisitor *visitor) override {
    if (visitor->visit(this)) {
      EXPR->accept(visitor);
    }
    visitor->endVisit(this);
  }

protected:
  std::ostream &print(std::ostream &out) const override {
    out << "old(" << *EXPR << ")";
    return out;
  }
};

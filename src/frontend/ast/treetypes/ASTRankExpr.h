#pragma once

#include "ASTExpr.h"
#include "ASTVisitor.h"

/*! \brief VERUTOP: rank(E), the size of a sum value, as an int.
 *
 * rank(C(x1, ..., xn)) is one more than the sum of the ranks of the fields
 * that are sum values, so a part is always smaller than the whole.  It turns
 * a sum value into a measure: `decreases rank(t)`.  It appears only in
 * specifications.
 */
class ASTRankExpr : public ASTExpr {
  std::shared_ptr<ASTExpr> EXPR;

public:
  explicit ASTRankExpr(std::shared_ptr<ASTExpr> e) : EXPR(std::move(e)) {}
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
    out << "rank(" << *EXPR << ")";
    return out;
  }
};

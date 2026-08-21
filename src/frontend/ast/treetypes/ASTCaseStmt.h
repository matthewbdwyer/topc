#pragma once

#include "ASTCaseArm.h"
#include "ASTExpr.h"
#include "ASTStmt.h"

#include <vector>

/*! \brief Class for a case statement (pattern-match on a sum type).
 *
 * Example:
 *   case p of {
 *     Some(v) -> output v;
 *     None    -> output 0;
 *   }
 */
class ASTCaseStmt : public ASTStmt {
  std::shared_ptr<ASTExpr> CASE_EXPR;
  std::vector<std::shared_ptr<ASTCaseArm>> ARMS;

public:
  std::vector<std::shared_ptr<ASTNode>> getChildren() override;
    ASTCaseStmt(std::shared_ptr<ASTExpr> caseExpr,
              std::vector<std::shared_ptr<ASTCaseArm>> arms)
      : CASE_EXPR(std::move(caseExpr)), ARMS(std::move(arms)) {}
    ASTExpr *getCaseExpr() const { return CASE_EXPR.get(); }
  std::vector<ASTCaseArm *> getArms() const;
  void accept(ASTVisitor *visitor) override;

  bool replaceChild(ASTNode *oldChild,
                    std::shared_ptr<ASTNode> newChild) override {
    return astReplaceSlot(CASE_EXPR, oldChild, newChild);
  }

protected:
  std::ostream &print(std::ostream &out) const override;
};

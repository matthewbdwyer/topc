#pragma once

#include "ASTExpr.h"

/*! \brief Class for function call expressions
 */
class ASTFunAppExpr : public ASTExpr {
  std::shared_ptr<ASTExpr> FUN;
  std::vector<std::shared_ptr<ASTExpr>> ACTUALS;

public:
  std::vector<std::shared_ptr<ASTNode>> getChildren() override;
  ASTFunAppExpr(std::shared_ptr<ASTExpr> FUN,
                std::vector<std::shared_ptr<ASTExpr>> ACTUALS);
  ASTExpr *getFunction() const { return FUN.get(); }
  std::vector<ASTExpr *> getActuals() const;
  void accept(ASTVisitor *visitor) override;

  bool replaceChild(ASTNode *oldChild,
                    std::shared_ptr<ASTNode> newChild) override {
    return astReplaceSlot(FUN, oldChild, newChild) ||
           astReplaceVec(ACTUALS, oldChild, newChild);
  }

protected:
  std::ostream &print(std::ostream &out) const override;
};

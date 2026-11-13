#pragma once

#include "ASTExpr.h"
#include "ASTStmt.h"

/*! \brief Class for a while loop.
 */
class ASTWhileStmt : public ASTStmt {
  std::shared_ptr<ASTExpr> COND;
  std::shared_ptr<ASTStmt> BODY;
  std::vector<std::shared_ptr<ASTExpr>> INVARIANTS; // VERUTOP
  std::shared_ptr<ASTExpr> DECREASES;               // VERUTOP

public:
  std::vector<std::shared_ptr<ASTNode>> getChildren() override;
  ASTWhileStmt(std::shared_ptr<ASTExpr> COND, std::shared_ptr<ASTStmt> BODY)
      : COND(COND), BODY(BODY) {}
  ASTExpr *getCondition() const { return COND.get(); }
  ASTStmt *getBody() const { return BODY.get(); }
  void setInvariants(std::vector<std::shared_ptr<ASTExpr>> inv) {
    INVARIANTS = std::move(inv);
  }
  void setDecreases(std::shared_ptr<ASTExpr> d) { DECREASES = std::move(d); }
  ASTExpr *getDecreases() const { return DECREASES.get(); }
  std::vector<ASTExpr *> getInvariants() const {
    std::vector<ASTExpr *> v;
    for (auto &e : INVARIANTS) v.push_back(e.get());
    return v;
  }
  void accept(ASTVisitor *visitor) override;

  bool replaceChild(ASTNode *oldChild,
                    std::shared_ptr<ASTNode> newChild) override {
    return astReplaceSlot(COND, oldChild, newChild) ||
           astReplaceSlot(BODY, oldChild, newChild);
  }

protected:
  std::ostream &print(std::ostream &out) const override;
};

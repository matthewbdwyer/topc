#pragma once

#include "ASTExpr.h"

/*! \brief Class for a range expression (SOP stub — semantics in sopc).
 *
 * Examples: `1 .. 10` or `1 .. 10 by 2`
 */
class ASTRangeExpr : public ASTExpr {
  std::shared_ptr<ASTExpr> LO;
  std::shared_ptr<ASTExpr> HI;
  std::shared_ptr<ASTExpr> STEP; // nullable

public:
  std::vector<std::shared_ptr<ASTNode>> getChildren() override;
  ASTRangeExpr(std::shared_ptr<ASTExpr> lo, std::shared_ptr<ASTExpr> hi,
               std::shared_ptr<ASTExpr> step = nullptr)
      : LO(std::move(lo)), HI(std::move(hi)), STEP(std::move(step)) {}
  ASTExpr *getLo() const { return LO.get(); }
  ASTExpr *getHi() const { return HI.get(); }
  ASTExpr *getStep() const { return STEP.get(); } // nullptr if absent
  void accept(ASTVisitor *visitor) override;

protected:
  std::ostream &print(std::ostream &out) const override;
};

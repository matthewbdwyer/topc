#include "ASTRangeExpr.h"
#include "ASTVisitor.h"

void ASTRangeExpr::accept(ASTVisitor *visitor) {
  if (visitor->visit(this)) {
    getLo()->accept(visitor);
    getHi()->accept(visitor);
    if (getStep())
      getStep()->accept(visitor);
  }
  visitor->endVisit(this);
}

std::ostream &ASTRangeExpr::print(std::ostream &out) const {
  out << *LO << " .. " << *HI;
  if (STEP)
    out << " by " << *STEP;
  return out;
}

std::vector<std::shared_ptr<ASTNode>> ASTRangeExpr::getChildren() {
  std::vector<std::shared_ptr<ASTNode>> children;
  children.push_back(LO);
  children.push_back(HI);
  if (STEP)
    children.push_back(STEP);
  return children;
}

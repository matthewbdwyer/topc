#include "ASTBorrowExpr.h"
#include "ASTVisitor.h"

void ASTBorrowExpr::accept(ASTVisitor *visitor) {
  if (visitor->visit(this)) {
    getVar()->accept(visitor);
  }
  visitor->endVisit(this);
}

std::ostream &ASTBorrowExpr::print(std::ostream &out) const {
  out << "&" << *getVar();
  return out;
}

std::vector<std::shared_ptr<ASTNode>> ASTBorrowExpr::getChildren() {
  std::vector<std::shared_ptr<ASTNode>> children;
  children.push_back(VAR);
  return children;
}
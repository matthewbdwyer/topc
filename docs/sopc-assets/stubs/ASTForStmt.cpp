#include "ASTForStmt.h"
#include "ASTVisitor.h"

void ASTForStmt::accept(ASTVisitor *visitor) {
  if (visitor->visit(this)) {
    getVar()->accept(visitor);
    getIterable()->accept(visitor);
    getBody()->accept(visitor);
  }
  visitor->endVisit(this);
}

std::ostream &ASTForStmt::print(std::ostream &out) const {
  out << "for (" << *VAR << " : " << *ITERABLE << ") " << *BODY;
  return out;
}

std::vector<std::shared_ptr<ASTNode>> ASTForStmt::getChildren() {
  std::vector<std::shared_ptr<ASTNode>> children;
  children.push_back(VAR);
  children.push_back(ITERABLE);
  children.push_back(BODY);
  return children;
}

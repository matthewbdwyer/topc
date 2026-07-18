#include "ASTFieldAccessExpr.h"
#include "ASTVisitor.h"

void ASTFieldAccessExpr::accept(ASTVisitor *visitor) {
  if (visitor->visit(this)) {
    getBase()->accept(visitor);
  }
  visitor->endVisit(this);
}

std::ostream &ASTFieldAccessExpr::print(std::ostream &out) const {
  out << *getBase() << "." << FIELD;
  return out;
}

std::vector<std::shared_ptr<ASTNode>> ASTFieldAccessExpr::getChildren() {
  return {BASE};
}

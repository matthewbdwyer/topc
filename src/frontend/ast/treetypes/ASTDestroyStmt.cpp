#include "ASTDestroyStmt.h"
#include "ASTVisitor.h"

void ASTDestroyStmt::accept(ASTVisitor *visitor) {
  // Leaf node: no children to visit.
  visitor->visit(this);
  visitor->endVisit(this);
}

std::ostream &ASTDestroyStmt::print(std::ostream &out) const {
  out << "destroy " << VAR->getName() << ";";
  return out;
}

std::vector<std::shared_ptr<ASTNode>> ASTDestroyStmt::getChildren() {
  return {}; // synthetic leaf — no sub-tree traversal
}

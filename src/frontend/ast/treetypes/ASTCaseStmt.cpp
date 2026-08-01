#include "ASTCaseStmt.h"
#include "ASTVisitor.h"
#include "ASTinternal.h"

void ASTCaseStmt::accept(ASTVisitor *visitor) {
  if (visitor->visit(this)) {
    getCaseExpr()->accept(visitor);
    for (auto arm : getArms()) {
      arm->accept(visitor);
    }
  }
  visitor->endVisit(this);
}

std::ostream &ASTCaseStmt::print(std::ostream &out) const {
  out << "case " << *CASE_EXPR << " of {";
  for (auto &arm : ARMS)
    out << " " << *arm << ";";
  out << " }";
  return out;
}

std::vector<std::shared_ptr<ASTNode>> ASTCaseStmt::getChildren() {
  std::vector<std::shared_ptr<ASTNode>> children;
  children.push_back(CASE_EXPR);
  for (auto &arm : ARMS)
    children.push_back(arm);
  return children;
}

std::vector<ASTCaseArm *> ASTCaseStmt::getArms() const {
  return rawRefs(ARMS);
}

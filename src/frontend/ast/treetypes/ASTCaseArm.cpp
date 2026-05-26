#include "ASTCaseArm.h"
#include "ASTVisitor.h"
#include "ASTinternal.h"

void ASTCaseArm::accept(ASTVisitor *visitor) {
  if (visitor->visit(this)) {
    for (auto b : getBindings()) {
      b->accept(visitor);
    }
    getBody()->accept(visitor);
  }
  visitor->endVisit(this);
}

std::ostream &ASTCaseArm::print(std::ostream &out) const {
  out << TAG;
  if (!BINDINGS.empty()) {
    out << "(";
    bool first = true;
    for (auto &b : BINDINGS) {
      if (!first)
        out << ", ";
      out << *b;
      first = false;
    }
    out << ")";
  }
  out << " -> " << *BODY;
  return out;
}

std::vector<std::shared_ptr<ASTNode>> ASTCaseArm::getChildren() {
  std::vector<std::shared_ptr<ASTNode>> children;
  for (auto &b : BINDINGS)
    children.push_back(b);
  children.push_back(BODY);
  return children;
}

std::vector<ASTDeclNode *> ASTCaseArm::getBindings() const {
  return rawRefs(BINDINGS);
}

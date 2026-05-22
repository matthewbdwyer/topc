#include "ASTSumVariant.h"
#include "ASTVisitor.h"
#include "ASTinternal.h"

void ASTSumVariant::accept(ASTVisitor *visitor) {
  if (visitor->visit(this)) {
    for (auto p : getParams()) {
      p->accept(visitor);
    }
  }
  visitor->endVisit(this);
}

std::ostream &ASTSumVariant::print(std::ostream &out) const {
  out << TAG;
  if (!PARAMS.empty()) {
    out << "(";
    bool first = true;
    for (auto &p : PARAMS) {
      if (!first)
        out << ", ";
      out << *p;
      first = false;
    }
    out << ")";
  }
  return out;
}

std::vector<std::shared_ptr<ASTNode>> ASTSumVariant::getChildren() {
  std::vector<std::shared_ptr<ASTNode>> children;
  for (auto &p : PARAMS)
    children.push_back(p);
  return children;
}

std::vector<ASTDeclNode *> ASTSumVariant::getParams() const {
  return rawRefs(PARAMS);
}

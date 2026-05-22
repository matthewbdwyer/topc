#include "ASTSumTypeDecl.h"
#include "ASTVisitor.h"
#include "ASTinternal.h"

void ASTSumTypeDecl::accept(ASTVisitor *visitor) {
  if (visitor->visit(this)) {
    for (auto v : getVariants()) {
      v->accept(visitor);
    }
  }
  visitor->endVisit(this);
}

std::ostream &ASTSumTypeDecl::print(std::ostream &out) const {
  out << "type " << NAME << " = ";
  bool first = true;
  for (auto &v : VARIANTS) {
    if (!first)
      out << " | ";
    out << *v;
    first = false;
  }
  out << ";";
  return out;
}

std::vector<std::shared_ptr<ASTNode>> ASTSumTypeDecl::getChildren() {
  std::vector<std::shared_ptr<ASTNode>> children;
  for (auto &v : VARIANTS)
    children.push_back(v);
  return children;
}

std::vector<ASTSumVariant *> ASTSumTypeDecl::getVariants() const {
  return rawRefs(VARIANTS);
}

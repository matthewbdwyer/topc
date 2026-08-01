#include "ASTSumCtorExpr.h"
#include "ASTVisitor.h"
#include "ASTinternal.h"

std::vector<ASTExpr *> ASTSumCtorExpr::getArgs() const {
  return rawRefs(ARGS);
}

void ASTSumCtorExpr::accept(ASTVisitor *visitor) {
  if (visitor->visit(this)) {
    for (auto *arg : getArgs()) {
      arg->accept(visitor);
    }
  }
  visitor->endVisit(this);
}

std::ostream &ASTSumCtorExpr::print(std::ostream &out) const {
  out << TAG;
  if (!ARGS.empty()) {
    out << "(";
    bool first = true;
    for (auto *arg : getArgs()) {
      if (!first)
        out << ", ";
      out << *arg;
      first = false;
    }
    out << ")";
  }
  return out;
} // LCOV_EXCL_LINE

std::vector<std::shared_ptr<ASTNode>> ASTSumCtorExpr::getChildren() {
  std::vector<std::shared_ptr<ASTNode>> children;
  for (auto &arg : ARGS)
    children.push_back(arg);
  return children;
}

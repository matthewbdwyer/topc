#include "ASTProgram.h"
#include "ASTVisitor.h"
#include "ASTinternal.h"

ASTProgram::ASTProgram(std::vector<std::shared_ptr<ASTSumTypeDecl>> typedecls,
                       std::vector<std::shared_ptr<ASTFunction>> functions) {
  for (auto &td : typedecls)
    this->TYPEDECLS.push_back(td);
  for (auto &func : functions)
    this->FUNCTIONS.push_back(func);
}

std::vector<ASTSumTypeDecl *> ASTProgram::getTypedecls() const {
  return rawRefs(TYPEDECLS);
}

std::vector<ASTFunction *> ASTProgram::getFunctions() const {
  return rawRefs(FUNCTIONS);
}

ASTFunction *ASTProgram::findFunctionByName(std::string name) {
  for (auto fn : getFunctions()) {
    if (fn->getName() == name) {
      return fn;
    }
  }
  return nullptr;
}

void ASTProgram::accept(ASTVisitor *visitor) {
  if (visitor->visit(this)) {
    for (auto td : getTypedecls()) {
      td->accept(visitor);
    }
    for (auto f : getFunctions()) {
      f->accept(visitor);
    }
  }
  visitor->endVisit(this);
}

std::ostream &ASTProgram::print(std::ostream &out) const {
  out << getName();
  return out;
} // LCOV_EXCL_LINE

std::vector<std::shared_ptr<ASTNode>> ASTProgram::getChildren() {
  std::vector<std::shared_ptr<ASTNode>> children;
  for (auto &td : TYPEDECLS)
    children.push_back(td);
  for (auto &function : FUNCTIONS)
    children.push_back(function);
  return children;
}



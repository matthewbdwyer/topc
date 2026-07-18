#include "AbsentFieldChecker.h"
#include "SemanticError.h"
#include "TopAbsentField.h"
#include "TopVar.h"

#include <sstream>

void AbsentFieldChecker::check(ASTProgram *p, Unifier *u) {
  AbsentFieldChecker visitor(u);
  p->accept(&visitor);
}

void AbsentFieldChecker::endVisit(ASTFieldAccessExpr *element) {
  auto typeVar = std::make_shared<TopVar>(element);
  auto inferredType = unifier->inferred(typeVar);

  if (std::dynamic_pointer_cast<TopAbsentField>(inferredType) != nullptr) {
    std::stringstream sstream;
    sstream << *element;
    throw SemanticError("Access to absent field on line " +
                        std::to_string(element->getLine()) + " in column " +
                        std::to_string(element->getColumn()) + ": " +
                        sstream.str());
  }
}

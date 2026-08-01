#include "CheckAssignable.h"
#include "PrettyPrinter.h"
#include "SemanticError.h"
#include "../SemanticLogging.h"

#include <sstream>

#include "loguru.hpp"

namespace {

// Return true if expression has an l-value
bool isAssignable(ASTExpr *e) {
  if (dynamic_cast<ASTVariableExpr *>(e))
    return true;
  return false;
}

} // namespace

void CheckAssignable::endVisit(ASTAssignStmt *element) {
  SEMANTIC_LOG(3, "assignability") << "assignment=" << *element;

  if (isAssignable(element->getLHS()))
    return;

  // Assigning through a pointer is also permitted
  if (dynamic_cast<ASTDeRefExpr *>(element->getLHS()))
    return;

  std::ostringstream oss;
  oss << "Assignment error on line " << element->getLine() << ": ";
  oss << *element->getLHS() << " not an l-value\n";
  throw SemanticError(oss.str());
}

void CheckAssignable::endVisit(ASTBorrowExpr *element) {
  SEMANTIC_LOG(3, "assignability") << "borrow=" << *element;

  if (isAssignable(element->getVar()))
    return;

  std::ostringstream oss;
  oss << "Address of error on line " << element->getLine() << ": ";
  oss << *element->getVar() << " not an l-value\n";
  throw SemanticError(oss.str());
}

void CheckAssignable::check(ASTProgram *p) {
  SEMANTIC_LOG(1, "assignability") << "start";
  CheckAssignable visitor;
  p->accept(&visitor);
  SEMANTIC_LOG(1, "assignability") << "complete";
}

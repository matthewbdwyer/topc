#include "CheckRecordFields.h"

#include "SemanticError.h"

#include <sstream>
#include <unordered_set>

#include "loguru.hpp"

void CheckRecordFields::endVisit(ASTRecordExpr *element) {
  LOG_S(1) << "Checking record fields of " << *element;

  std::unordered_set<std::string> seen;
  for (const auto &name : element->getFieldNames()) {
    if (!seen.insert(name).second) {
      std::ostringstream oss;
      oss << "Record error on line " << element->getLine() << ": ";
      oss << "duplicate field '" << name << "'";
      throw SemanticError(oss.str());
    }
  }
}

void CheckRecordFields::check(ASTProgram *p) {
  LOG_S(1) << "Checking record field uniqueness";
  CheckRecordFields visitor;
  p->accept(&visitor);
}

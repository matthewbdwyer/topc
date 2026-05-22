#include "CheckConstructorCase.h"
#include "PrettyPrinter.h"
#include "SemanticError.h"

#include <cctype>
#include <sstream>

#include "loguru.hpp"

bool CheckConstructorCase::startsUppercase(const std::string &s) {
  return !s.empty() && std::isupper(static_cast<unsigned char>(s[0]));
}

bool CheckConstructorCase::startsLowercase(const std::string &s) {
  return !s.empty() && std::islower(static_cast<unsigned char>(s[0]));
}

void CheckConstructorCase::endVisit(ASTSumTypeDecl *element) {
  isTopProgram = true;

  // Rule 1: sum type name must start uppercase
  if (!startsUppercase(element->getName())) {
    std::ostringstream oss;
    oss << "Naming error on line " << element->getLine()
        << ": sum type name '" << element->getName()
        << "' must start with an uppercase letter\n";
    throw SemanticError(oss.str());
  }

  // Rule 2: constructor tags must start uppercase
  for (auto sv : element->getVariants()) {
    if (!startsUppercase(sv->getTag())) {
      std::ostringstream oss;
      oss << "Naming error on line " << sv->getLine()
          << ": constructor tag '" << sv->getTag()
          << "' must start with an uppercase letter\n";
      throw SemanticError(oss.str());
    }
  }
}

void CheckConstructorCase::endVisit(ASTFunction *element) {
  if (!isTopProgram)
    return;

  // Rule 3: function names must start lowercase
  if (!startsLowercase(element->getName())) {
    std::ostringstream oss;
    oss << "Naming error on line " << element->getDecl()->getLine()
        << ": function name '" << element->getName()
        << "' must start with a lowercase letter\n";
    throw SemanticError(oss.str());
  }

  // Rule 4: formal parameter names must start lowercase
  for (auto f : element->getFormals()) {
    if (!startsLowercase(f->getName())) {
      std::ostringstream oss;
      oss << "Naming error on line " << f->getLine()
          << ": parameter name '" << f->getName()
          << "' must start with a lowercase letter\n";
      throw SemanticError(oss.str());
    }
  }
}

void CheckConstructorCase::endVisit(ASTDeclStmt *element) {
  if (!isTopProgram)
    return;

  // Rule 4: local variable names must start lowercase
  for (auto v : element->getVars()) {
    if (!startsLowercase(v->getName())) {
      std::ostringstream oss;
      oss << "Naming error on line " << v->getLine()
          << ": variable name '" << v->getName()
          << "' must start with a lowercase letter\n";
      throw SemanticError(oss.str());
    }
  }
}

void CheckConstructorCase::check(ASTProgram *p) {
  LOG_S(1) << "Checking constructor/identifier casing";
  CheckConstructorCase visitor;
  p->accept(&visitor);
}

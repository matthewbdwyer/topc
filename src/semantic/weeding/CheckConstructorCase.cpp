#include "CheckConstructorCase.h"
#include "../SemanticLogging.h"
#include "PrettyPrinter.h"
#include "SemanticError.h"

#include <cctype>
#include <sstream>

#include "loguru.hpp"

// NOTE: The lexer already enforces identifier case through distinct token
// classes -- CONID (uppercase-initial) for type and constructor names, and
// IDENTIFIER (lowercase-initial) for functions, parameters, and locals. A
// program that violates a rule below is therefore a *parse* error and never
// reaches this pass. These checks are retained as defensive documentation of
// the naming discipline, but their rejection bodies are unreachable from any
// parsed program, so they are marked LCOV_EXCL to keep coverage honest.

bool CheckConstructorCase::startsUppercase(const std::string &s) {
  return !s.empty() && std::isupper(static_cast<unsigned char>(s[0]));
}

bool CheckConstructorCase::startsLowercase(const std::string &s) {
  return !s.empty() && std::islower(static_cast<unsigned char>(s[0]));
}

void CheckConstructorCase::endVisit(ASTSumTypeDecl *element) {
  // Rule 1: sum type name must start uppercase
  if (!startsUppercase(element->getName())) {
    // LCOV_EXCL_START -- unreachable: lexer requires CONID for type names
    std::ostringstream oss;
    oss << "Naming error on line " << element->getLine()
        << ": sum type name '" << element->getName()
        << "' must start with an uppercase letter\n";
    throw SemanticError(oss.str());
    // LCOV_EXCL_STOP
  }

  // Rule 2: constructor tags must start uppercase
  for (auto sv : element->getVariants()) {
    if (!startsUppercase(sv->getTag())) {
      // LCOV_EXCL_START -- unreachable: lexer requires CONID for constructor tags
      std::ostringstream oss;
      oss << "Naming error on line " << sv->getLine()
          << ": constructor tag '" << sv->getTag()
          << "' must start with an uppercase letter\n";
      throw SemanticError(oss.str());
      // LCOV_EXCL_STOP
    }
  }
}

void CheckConstructorCase::endVisit(ASTFunction *element) {
  // Rule 3: function names must start lowercase
  if (!startsLowercase(element->getName())) {
    // LCOV_EXCL_START -- unreachable: lexer requires IDENTIFIER for function names
    std::ostringstream oss;
    oss << "Naming error on line " << element->getDecl()->getLine()
        << ": function name '" << element->getName()
        << "' must start with a lowercase letter\n";
    throw SemanticError(oss.str());
    // LCOV_EXCL_STOP
  }

  // Rule 4: formal parameter names must start lowercase
  for (auto f : element->getFormals()) {
    if (!startsLowercase(f->getName())) {
      // LCOV_EXCL_START -- unreachable: lexer requires IDENTIFIER for parameters
      std::ostringstream oss;
      oss << "Naming error on line " << f->getLine()
          << ": parameter name '" << f->getName()
          << "' must start with a lowercase letter\n";
      throw SemanticError(oss.str());
      // LCOV_EXCL_STOP
    }
  }
}

void CheckConstructorCase::endVisit(ASTDeclStmt *element) {
  // Rule 4: local variable names must start lowercase
  for (auto v : element->getVars()) {
    if (!startsLowercase(v->getName())) {
      // LCOV_EXCL_START -- unreachable: lexer requires IDENTIFIER for locals
      std::ostringstream oss;
      oss << "Naming error on line " << v->getLine()
          << ": variable name '" << v->getName()
          << "' must start with a lowercase letter\n";
      throw SemanticError(oss.str());
      // LCOV_EXCL_STOP
    }
  }
}

void CheckConstructorCase::check(ASTProgram *p) {
  SEMANTIC_LOG(1, "identifier-case") << "start";
  CheckConstructorCase visitor;
  p->accept(&visitor);
  SEMANTIC_LOG(1, "identifier-case") << "complete";
}

#include "CheckPatternTypes.h"
#include "ASTCtorPattern.h"
#include "ASTPattern.h"
#include "ASTRecordPattern.h"
#include "ASTVarPattern.h"
#include "ASTWildcardPattern.h"
#include "ASTCaseArm.h"
#include "ASTCaseStmt.h"
#include "ASTProgram.h"
#include "SemanticError.h"

#include <sstream>
#include <string>

#include "loguru.hpp"

bool CheckPatternTypes::visit(ASTProgram *element) {
  // Build the constructor-arity map from all type declarations so that nested
  // constructor patterns can be validated against declared arities.
  for (auto td : element->getTypedecls()) {
    for (auto sv : td->getVariants()) {
      constructorArity[sv->getTag()] =
          static_cast<int>(sv->getParams().size());
    }
  }
  return true;
}

void CheckPatternTypes::checkPattern(ASTPattern *pat, int line) {
  if (auto *cp = dynamic_cast<ASTCtorPattern *>(pat)) {
    const std::string &tag = cp->getTag();

    // Rule 1: constructor tag must be declared.
    if (constructorArity.find(tag) == constructorArity.end()) {
      std::ostringstream oss;
      oss << "Pattern error on line " << line
          << ": unknown constructor '" << tag << "' in nested pattern\n";
      throw SemanticError(oss.str());
    }

    // Rule 2: sub-pattern count must match declared arity.
    int declArity = constructorArity.at(tag);
    int patArity  = static_cast<int>(cp->getSubPatterns().size());
    if (patArity != declArity) {
      std::ostringstream oss;
      oss << "Pattern error on line " << line
          << ": constructor '" << tag << "' expects " << declArity
          << " pattern(s) but " << patArity << " provided in nested pattern\n";
      throw SemanticError(oss.str());
    }

    // Recurse into sub-patterns.
    for (auto *sp : cp->getSubPatterns())
      checkPattern(sp, line);

  } else if (auto *rp = dynamic_cast<ASTRecordPattern *>(pat)) {
    // Recurse into field sub-patterns.
    for (auto &[fname, sp] : rp->getFields())
      checkPattern(sp.get(), line);
  }
  // ASTVarPattern and ASTWildcardPattern have no structural sub-patterns.
}

void CheckPatternTypes::endVisit(ASTCaseStmt *element) {
  for (auto *arm : element->getArms()) {
    for (auto *pat : arm->getPatterns())
      checkPattern(pat, arm->getLine());
  }
}

void CheckPatternTypes::check(ASTProgram *p) {
  LOG_S(1) << "Checking nested pattern types";
  CheckPatternTypes visitor;
  p->accept(&visitor);
}

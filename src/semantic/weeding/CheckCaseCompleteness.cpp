#include "CheckCaseCompleteness.h"
#include "PrettyPrinter.h"
#include "SemanticError.h"

#include <set>
#include <sstream>

#include "loguru.hpp"

bool CheckCaseCompleteness::visit(ASTProgram *element) {
  // Build the constructor arity map from all type declarations.
  for (auto td : element->getTypedecls()) {
    for (auto sv : td->getVariants()) {
      constructorArity[sv->getTag()] =
          static_cast<int>(sv->getParams().size());
      constructorType[sv->getTag()] = td->getName();
    }
  }
  return true;
}

void CheckCaseCompleteness::endVisit(ASTCaseStmt *element) {
  std::set<std::string> seenTags;

  for (auto arm : element->getArms()) {
    const std::string &tag = arm->getTag();

    // Rule 1: constructor must be declared
    if (constructorArity.find(tag) == constructorArity.end()) {
      std::ostringstream oss;
      oss << "Case error on line " << arm->getLine()
          << ": unknown constructor '" << tag << "' in case arm\n";
      throw SemanticError(oss.str());
    }

    // Rule 2: binding arity matches declaration
    int declArity = constructorArity.at(tag);
    int armArity = static_cast<int>(arm->getBindings().size());
    if (armArity != declArity) {
      std::ostringstream oss;
      oss << "Case error on line " << arm->getLine()
          << ": constructor '" << tag << "' expects " << declArity
          << " binding(s) but arm provides " << armArity << "\n";
      throw SemanticError(oss.str());
    }

    // Rule 3: no duplicate arm for the same constructor
    if (seenTags.count(tag)) {
      std::ostringstream oss;
      oss << "Case error on line " << arm->getLine()
          << ": constructor '" << tag << "' appears more than once in case\n";
      throw SemanticError(oss.str());
    }
    seenTags.insert(tag);
  }

  // Rule 4: all constructors of the scrutinee's sum type must be covered.
  // We determine which sum type is being matched by looking at the first arm's
  // constructor (if any arms exist).
  if (element->getArms().empty()) {
    std::ostringstream oss;
    oss << "Case error on line " << element->getLine()
        << ": case statement has no arms\n";
    throw SemanticError(oss.str());
  }

  // Find which type the arms belong to (use first arm's tag).
  const std::string &firstTag = element->getArms()[0]->getTag();
  const std::string &typeName = constructorType.at(firstTag);

  // Collect all constructors of that type.
  std::set<std::string> allCtors;
  for (auto &[tag, tname] : constructorType) {
    if (tname == typeName)
      allCtors.insert(tag);
  }

  // Check completeness.
  for (const auto &ctor : allCtors) {
    if (!seenTags.count(ctor)) {
      std::ostringstream oss;
      oss << "Case error on line " << element->getLine()
          << ": case is not exhaustive — constructor '" << ctor
          << "' of type '" << typeName << "' is not covered\n";
      throw SemanticError(oss.str());
    }
  }

  // Also check that all used tags belong to the same type.
  for (const auto &tag : seenTags) {
    if (constructorType.count(tag) &&
        constructorType.at(tag) != typeName) {
      std::ostringstream oss;
      oss << "Case error: arm constructor '" << tag
          << "' belongs to type '" << constructorType.at(tag)
          << "' but other arms use type '" << typeName << "'\n";
      throw SemanticError(oss.str());
    }
  }
}

void CheckCaseCompleteness::check(ASTProgram *p) {
  LOG_S(1) << "Checking case completeness";
  CheckCaseCompleteness visitor;
  p->accept(&visitor);
}

#include "CheckCaseCompleteness.h"
#include "../SemanticLogging.h"
#include "ASTCtorPattern.h"
#include "ASTVarPattern.h"
#include "ASTWildcardPattern.h"
#include "PrettyPrinter.h"
#include "SemanticError.h"

#include <map>
#include <set>
#include <sstream>
#include <vector>

#include "loguru.hpp"

// ---------------------------------------------------------------------------
// Pattern analysis helpers
// ---------------------------------------------------------------------------

bool CheckCaseCompleteness::isIrrefutablePattern(ASTPattern *p) {
  return dynamic_cast<ASTVarPattern *>(p) != nullptr ||
         dynamic_cast<ASTWildcardPattern *>(p) != nullptr;
}

bool CheckCaseCompleteness::allIrrefutablePayload(ASTCaseArm *arm) {
  for (auto *p : arm->getPatterns())
    if (!isIrrefutablePattern(p))
      return false;
  return true; // vacuously true for 0-arity constructors
}

bool CheckCaseCompleteness::patternsIdentical(ASTPattern *a, ASTPattern *b) {
  // Wildcards are always identical to each other.
  if (dynamic_cast<ASTWildcardPattern *>(a) &&
      dynamic_cast<ASTWildcardPattern *>(b))
    return true;

  // Var patterns are identical when they share the same name.
  auto *va = dynamic_cast<ASTVarPattern *>(a);
  auto *vb = dynamic_cast<ASTVarPattern *>(b);
  if (va && vb)
    return va->getName() == vb->getName();

  // Constructor patterns: same tag and pairwise identical sub-patterns.
  auto *ca = dynamic_cast<ASTCtorPattern *>(a);
  auto *cb = dynamic_cast<ASTCtorPattern *>(b);
  if (ca && cb) {
    if (ca->getTag() != cb->getTag())
      return false;
    auto sa = ca->getSubPatterns();
    auto sb = cb->getSubPatterns();
    if (sa.size() != sb.size())
      return false;
    for (std::size_t i = 0; i < sa.size(); ++i)
      if (!patternsIdentical(sa[i], sb[i]))
        return false;
    return true;
  }

  return false; // different pattern kinds
}

bool CheckCaseCompleteness::allPatternsIdentical(ASTCaseArm *a,
                                                  ASTCaseArm *b) {
  auto pa = a->getPatterns();
  auto pb = b->getPatterns();
  if (pa.size() != pb.size())
    return false;
  for (std::size_t i = 0; i < pa.size(); ++i)
    if (!patternsIdentical(pa[i], pb[i]))
      return false;
  return true; // vacuously true for 0-arity constructors
}

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
  // Track all arms seen so far, keyed by constructor tag.
  std::map<std::string, std::vector<ASTCaseArm *>> seenArms;

  for (auto arm : element->getArms()) {
    const std::string &tag = arm->getTag();

    // Rule 1: constructor must be declared
    if (constructorArity.find(tag) == constructorArity.end()) {
      std::ostringstream oss;
      oss << "Case error on line " << arm->getLine()
          << ": unknown constructor '" << tag << "' in case arm\n";
      throw SemanticError(oss.str());
    }

    // Rule 2: pattern arity matches declaration
    int declArity = constructorArity.at(tag);
    int armArity = static_cast<int>(arm->getPatterns().size());
    if (armArity != declArity) {
      std::ostringstream oss;
      oss << "Case error on line " << arm->getLine()
          << ": constructor '" << tag << "' expects " << declArity
          << " binding(s) but arm provides " << armArity << "\n";
      throw SemanticError(oss.str());
    }

    // Rule 3 (B3): redundancy check for duplicate-constructor arms.
    // An arm is unreachable if any earlier arm for the same constructor:
    //   (a) has entirely irrefutable payload patterns, OR
    //   (b) has syntactically identical payload patterns to the current arm.
    if (seenArms.count(tag)) {
      for (auto *prevArm : seenArms.at(tag)) {
        if (allIrrefutablePayload(prevArm) ||
            allPatternsIdentical(prevArm, arm)) {
          std::ostringstream oss;
          oss << "Case error on line " << arm->getLine()
              << ": unreachable case arm — constructor '" << tag
              << "' is already fully covered by an earlier arm\n";
          throw SemanticError(oss.str());
        }
      }
    }

    seenArms[tag].push_back(arm);
  }

  // Rule 4: all constructors of the case expression's sum type must be covered.
  // We determine which sum type is being matched by looking at the first arm's
  // constructor (if any arms exist).
  if (element->getArms().empty()) {
    // LCOV_EXCL_START -- unreachable: the grammar requires at least one CONID arm
    std::ostringstream oss;
    oss << "Case error on line " << element->getLine()
        << ": case statement has no arms\n";
    throw SemanticError(oss.str());
    // LCOV_EXCL_STOP
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

  // Check exhaustiveness: each constructor must have at least one arm.
  for (const auto &ctor : allCtors) {
    if (!seenArms.count(ctor)) {
      std::ostringstream oss;
      oss << "Case error on line " << element->getLine()
          << ": case is not exhaustive — constructor '" << ctor
          << "' of type '" << typeName << "' is not covered\n";
      throw SemanticError(oss.str());
    }
  }

  // Also check that all used tags belong to the same type.
  for (const auto &[tag, arms] : seenArms) {
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
  SEMANTIC_LOG(1, "case-completeness") << "start";
  CheckCaseCompleteness visitor;
  p->accept(&visitor);
  SEMANTIC_LOG(1, "case-completeness") << "complete";
}

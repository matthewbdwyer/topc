#pragma once

#include "ASTVisitor.h"

#include <map>
#include <string>

/*! \class CheckPatternTypes
 *  \brief Reject structural errors in nested case-arm patterns.
 *
 * Rules checked (apply recursively to all patterns in every arm):
 *  1. A constructor tag used inside a nested pattern must be declared in
 *     some sum type in the program.
 *  2. The sub-pattern count for a nested constructor pattern must match
 *     the constructor's declared arity.
 *
 * Note: top-level arm tags and their arities are already validated by
 * CheckCaseCompleteness.  This pass focuses exclusively on patterns that
 * appear *inside* the payload position(s) of an arm.
 */
class CheckPatternTypes : public ASTVisitor {
public:
  CheckPatternTypes() = default;
  static void check(ASTProgram *p);

  bool visit(ASTProgram *element) override;
  void endVisit(ASTCaseStmt *element) override;

private:
  void checkPattern(ASTPattern *pat, int line);

  std::map<std::string, int> constructorArity; ///< ctor tag -> declared arity
};

#pragma once

#include "ASTVisitor.h"

/*! \class CheckSumTypeNames
 *  \brief Reject duplicate sum-type names and duplicate constructor tags.
 *
 * Rules checked:
 *  1. No two sum type declarations in the same program may share a name.
 *  2. No two constructors (across *all* type declarations in the program) may
 *     share a tag, because constructor lookup is global.
 */
class CheckSumTypeNames : public ASTVisitor {
public:
  CheckSumTypeNames() = default;
  static void check(ASTProgram *p);
  virtual void endVisit(ASTProgram *element) override;
};

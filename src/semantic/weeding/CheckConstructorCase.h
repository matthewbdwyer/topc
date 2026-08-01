#pragma once

#include "ASTVisitor.h"

/*! \class CheckConstructorCase
 *  \brief Enforce identifier casing conventions for TOP programs.
 *
 * Rules checked:
 *  1. Sum type names must start with an uppercase letter.
 *  2. Constructor (variant) tags must start with an uppercase letter.
 *  3. Function names must start with a lowercase letter.
 *  4. Variable names (ASTDeclNode inside a function, for-loop, or case arm
 *     binding) must start with a lowercase letter.
 *
 * These checks only fire when there is at least one sum type declaration in
 * the program (i.e., a TOP program); they are silent on plain TOP programs.
 */
class CheckConstructorCase : public ASTVisitor {
public:
  CheckConstructorCase() = default;
  static void check(ASTProgram *p);

  virtual void endVisit(ASTSumTypeDecl *element) override;
  virtual void endVisit(ASTFunction *element) override;
  virtual void endVisit(ASTDeclStmt *element) override;

private:
  static bool startsUppercase(const std::string &s);
  static bool startsLowercase(const std::string &s);
};

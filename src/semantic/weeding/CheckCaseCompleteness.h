#pragma once

#include "ASTVisitor.h"
#include <map>
#include <string>

/*! \class CheckCaseCompleteness
 *  \brief Verify case arms are consistent with declared constructor arities.
 *
 * Rules checked:
 *  1. Every constructor name used in a case arm must be declared in a sum type
 *     in the same program.
 *  2. The number of binding variables in each arm must equal the arity of the
 *     corresponding constructor declaration.
 *  3. No constructor may appear more than once in the same case statement.
 *
 * Missing arms (incomplete coverage) are also flagged as hard errors because
 * they risk ownership leaks.
 */
class CheckCaseCompleteness : public ASTVisitor {
public:
  CheckCaseCompleteness() = default;
  static void check(ASTProgram *p);

  virtual bool visit(ASTProgram *element) override;
  virtual void endVisit(ASTCaseStmt *element) override;

private:
  // constructor tag -> arity (0 for no-payload)
  std::map<std::string, int> constructorArity;
  // constructor tag -> sum type name it belongs to
  std::map<std::string, std::string> constructorType;
};

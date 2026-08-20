#pragma once

#include "ASTVisitor.h"
#include <map>
#include <string>

/*! \class CheckCaseCompleteness
 *  \brief Verify case arms are exhaustive and non-redundant.
 *
 * Rules checked:
 *  1. Every constructor tag used in a case arm must be declared in a sum type
 *     in the same program.
 *  2. The number of payload patterns in each arm must equal the arity of the
 *     corresponding constructor declaration.
 *  3. If an earlier arm for the same top-level constructor has irrefutable
 *     payload patterns (wildcards or variable bindings only), any later arm
 *     for that constructor is unreachable → error.
 *     Two syntactically identical adjacent arms for the same constructor →
 *     the second is also unreachable → error.
 *  4. All constructors of the case expression's sum type must appear in at least
 *     one arm (exhaustiveness).
 *
 * The same well-formedness rules are also enforced on constructor *expressions*
 * (`Tag(e1,...,en)`), which are otherwise unchecked:
 *  5. A constructor expression's tag must be a declared constructor.
 *  6. The number of argument expressions must equal the constructor's arity.
 */
class CheckCaseCompleteness : public ASTVisitor {
public:
  CheckCaseCompleteness() = default;
  static void check(ASTProgram *p);

  virtual bool visit(ASTProgram *element) override;
  virtual void endVisit(ASTCaseStmt *element) override;
  virtual void endVisit(ASTSumCtorExpr *element) override;

private:
  // constructor tag -> arity (0 for no-payload)
  std::map<std::string, int> constructorArity;
  // constructor tag -> sum type name it belongs to
  std::map<std::string, std::string> constructorType;

  // Pattern analysis helpers (operate on ASTPattern* from ASTCaseArm).
  static bool isIrrefutablePattern(ASTPattern *p);
  static bool allIrrefutablePayload(ASTCaseArm *arm);
  static bool patternsIdentical(ASTPattern *a, ASTPattern *b);
  static bool allPatternsIdentical(ASTCaseArm *a, ASTCaseArm *b);
};


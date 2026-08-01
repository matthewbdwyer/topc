#include "TypeConstraintVisitor.h"
#include "ASTCaseStmt.h"
#include "ASTCtorPattern.h"
#include "ASTPattern.h"
#include "ASTSumCtorExpr.h"
#include "ASTSumTypeDecl.h"
#include "ASTSumVariant.h"
#include "ASTVarPattern.h"
#include "ASTWildcardPattern.h"
#include "TopAlpha.h"
#include "TopBorrowRef.h"
#include "TopFunction.h"
#include "TopInt.h"
#include "TopModeVar.h"
#include "TopOwningRef.h"
#include "ReferenceType.h"
#include "TopSumType.h"
#include "TopVar.h"

TypeConstraintVisitor::TypeConstraintVisitor(
    SymbolTable *st, std::shared_ptr<ConstraintHandler> handler)
  : symbolTable(st), constraintHandler(std::move(handler)) {};

/*! \fn astToVar
 *  \brief Convert an AST node to a type variable.
 *
 * Utility function that creates type variables and uses declaration nodes
 * as a canonical representative for program variables.  There are two case
 * that need to be checked: if the variable is local to a function or if
 * it is a function value.
 */
std::shared_ptr<TopType> TypeConstraintVisitor::astToVar(ASTNode *n) {
  if (auto ve = dynamic_cast<ASTVariableExpr *>(n)) {
    ASTDeclNode *canonical;
    if ((canonical = symbolTable->getLocal(ve->getName(), scope.top()))) {
      return std::make_shared<TopVar>(canonical);
    } else if ((canonical = symbolTable->getFunction(ve->getName()))) {
      return std::make_shared<TopVar>(canonical);
    }
  } // LCOV_EXCL_LINE

  return std::make_shared<TopVar>(n);
}

bool TypeConstraintVisitor::visit(ASTFunction *element) {
  scope.push(element->getDecl());
  return true;
}

/*! \brief Type constraints for function definition.
 *
 * Type rules for "main(X1, ..., Xn) { ... return E; }":
 *   [[X1]] = [[Xn]] = [[E]] = int
 * To express this we will equate all type variables to int.
 *
 * Type rules for "X(X1, ..., Xn) { ... return E; }":
 *   [[X]] = ([[X1]], ..., [[Xn]]) -> [[E]]
 */
void TypeConstraintVisitor::endVisit(ASTFunction *element) {
  if (element->getName() == "main") {
    std::vector<std::shared_ptr<TopType>> formals;
    for (auto &f : element->getFormals()) {
      formals.push_back(astToVar(f));
      // all formals are int
      constraintHandler->handle(astToVar(f), std::make_shared<TopInt>());
    }

    // Return is the last statement and must be int
    auto ret = dynamic_cast<ASTReturnStmt *>(element->getStmts().back());
    constraintHandler->handle(astToVar(ret->getArg()),
                              std::make_shared<TopInt>());

    constraintHandler->handle(
        astToVar(element->getDecl()),
        std::make_shared<TopFunction>(formals, astToVar(ret->getArg())));
  } else {
    std::vector<std::shared_ptr<TopType>> formals;
    for (auto &f : element->getFormals()) {
      formals.push_back(astToVar(f));
    }

    // Return is the last statement
    auto ret = dynamic_cast<ASTReturnStmt *>(element->getStmts().back());

    constraintHandler->handle(
        astToVar(element->getDecl()),
        std::make_shared<TopFunction>(formals, astToVar(ret->getArg())));
  }
}

/*! \brief Type constraints for numeric literal.
 *
 * Type rules for "I":
 *   [[I]] = int
 */
void TypeConstraintVisitor::endVisit(ASTNumberExpr *element) {
  constraintHandler->handle(astToVar(element), std::make_shared<TopInt>());
}

/*! \brief Type constraints for binary operator.
 *
 * Type rules for "E1 op E2":
 *   [[E1 op E2]] = int
 * and if "op" is not equality or disequality
 *   [[E1]] = [[E2]] = int
 * otherwise
 *   [[E1]] = [[E2]]
 */
void TypeConstraintVisitor::endVisit(ASTBinaryExpr *element) {
  auto op = element->getOp();
  auto intType = std::make_shared<TopInt>();

  // result type is integer
  constraintHandler->handle(astToVar(element), intType);

  if (op != "==" && op != "!=") {
    // operands are integer
    constraintHandler->handle(astToVar(element->getLeft()), intType);
    constraintHandler->handle(astToVar(element->getRight()), intType);
  } else {
    // operands have the same type
    constraintHandler->handle(astToVar(element->getLeft()),
                              astToVar(element->getRight()));
  }
}

/*! \brief Type constraints for input statement.
 *
 * Type rules for "input":
 *  [[input]] = int
 */
void TypeConstraintVisitor::endVisit(ASTInputExpr *element) {
  constraintHandler->handle(astToVar(element), std::make_shared<TopInt>());
}

/*! \brief Type constraints for function application.
 *
 * Type Rules for "E(E1, ..., En)":
 *  [[E]] = ([[E1]], ..., [[En]]) -> [[E(E1, ..., En)]]
 */
void TypeConstraintVisitor::endVisit(ASTFunAppExpr *element) {
  std::vector<std::shared_ptr<TopType>> actuals;
  for (auto &a : element->getActuals()) {
    actuals.push_back(astToVar(a));
  }
  constraintHandler->handle(
      astToVar(element->getFunction()),
      std::make_shared<TopFunction>(actuals, astToVar(element)));
}

/*! \brief Type constraints for heap allocation.
 *
 * Type Rule for "alloc E":
 *   [[alloc E]] = own&[[E]]
 */
void TypeConstraintVisitor::endVisit(ASTAllocExpr *element) {
  constraintHandler->handle(
      astToVar(element),
      std::make_shared<TopOwningRef>(astToVar(element->getInitializer())));
}

/*! \brief Type constraints for address of.
 *
 * TOP-only type rule for "&X":
 *   [[&X]] = borrow&[[X]]
 */
void TypeConstraintVisitor::endVisit(ASTBorrowExpr *element) {
  constraintHandler->handle(
      astToVar(element),
      std::make_shared<TopBorrowRef>(astToVar(element->getVar())));
}

/*! \brief Type constraints for pointer dereference.
 *
 * Type rule for "*E":
 *   [[E]] = ref&[[*E]]
 */
void TypeConstraintVisitor::endVisit(ASTDeRefExpr *element) {
  constraintHandler->handle(std::make_shared<ReferenceType>(std::make_shared<TopModeVar>(), astToVar(element)),
                            astToVar(element->getPtr()));
}

/*! \brief Type rules for assignments.
 *
 * Type rules for "E1 = E":
 *   [[E1]] = [[E2]]
 *
 * Type rules for "*E1 = E2":
 *   [[E1]] = ref&[[E2]]
 *
 * Note that these are slightly more general than the rules in the SPA book.
 * The first allows for record expressions on the left hand side and the second
 * allows for more complex assignments, e.g., "**p = &x"
 */
void TypeConstraintVisitor::endVisit(ASTAssignStmt *element) {
  // If this is an assignment through a pointer, use the second rule above
  if (auto lptr = dynamic_cast<ASTDeRefExpr *>(element->getLHS())) {
    constraintHandler->handle(
      std::make_shared<ReferenceType>(std::make_shared<TopModeVar>(), astToVar(element->getRHS())),
        astToVar(lptr->getPtr()));
  } else {
    constraintHandler->handle(astToVar(element->getLHS()),
                              astToVar(element->getRHS()));
  }
}

/*! \brief Type constraints for while loop.
 *
 * Type rules for "while (E) S":
 *   [[E]] = int
 */
void TypeConstraintVisitor::endVisit(ASTWhileStmt *element) {
  constraintHandler->handle(astToVar(element->getCondition()),
                            std::make_shared<TopInt>());
}

/*! \brief Type constraints for if statement.
 *
 * Type rules for "if (E) S1 else S2":
 *   [[E]] = int
 */
void TypeConstraintVisitor::endVisit(ASTIfStmt *element) {
  constraintHandler->handle(astToVar(element->getCondition()),
                            std::make_shared<TopInt>());
}

/*! \brief Type constraints for output statement.
 *
 * Type rules for "output E":
 *   [[E]] = int
 */
void TypeConstraintVisitor::endVisit(ASTOutputStmt *element) {
  constraintHandler->handle(astToVar(element->getArg()),
                            std::make_shared<TopInt>());
}

/*! \brief Type constraints for error statement.
 *
 * Type rules for "error E":
 *   [[E]] = int
 */
void TypeConstraintVisitor::endVisit(ASTErrorStmt *element) {
  constraintHandler->handle(astToVar(element->getArg()),
                            std::make_shared<TopInt>());
}

/*! \brief Skip constraint generation for sum type declarations.
 *
 * Sum type declarations define the structure used in case constraints;
 * they do not themselves produce type constraints.
 */
bool TypeConstraintVisitor::visit(ASTSumTypeDecl *element) {
  return false; // do not recurse into variant param decls
}

/*! \brief Type constraints for a sum type constructor expression.
 *
 * For "TAG" (nullary) or "TAG(e1,...,en)":
 *   1. [[TAG(e1,...,en)]] = SumType(TypeName, ...)
 *      where the sum type is determined by TAG's declaring type.
 *   2. For each argument e_i: [[e_i]] = [[variant_param_i]]
 */
void TypeConstraintVisitor::endVisit(ASTSumCtorExpr *element) {
  auto *variant = symbolTable->getConstructor(element->getTag());
  if (!variant)
    return; // unknown constructor caught by weeding

  auto *ownerDecl = symbolTable->getConstructorOwner(element->getTag());
  if (!ownerDecl)
    return;

  // Build the same TopSumType as endVisit(ASTCaseStmt*) does.
  std::vector<std::string> ctorNames;
  std::vector<std::shared_ptr<TopType>> payloads;
  std::map<std::string, int> arities;

  for (auto *v : ownerDecl->getVariants()) {
    const std::string &tag = v->getTag();
    ctorNames.push_back(tag);
    auto params = v->getParams();
    arities[tag] = static_cast<int>(params.size());
    for (auto *param : params) {
      payloads.push_back(astToVar(param));
    }
  }

  auto sumTy = std::make_shared<TopSumType>(ownerDecl->getName(), ctorNames,
                                            payloads, arities);
  constraintHandler->handle(astToVar(element), sumTy);

  // Constrain each argument to the corresponding variant parameter.
  auto variantParams = variant->getParams();
  auto args = element->getArgs();
  for (std::size_t i = 0; i < args.size() && i < variantParams.size(); i++) {
    constraintHandler->handle(astToVar(args[i]), astToVar(variantParams[i]));
  }
}

/*! \brief Type constraints for a case statement.
 *
 * For "case E of { C1(v1,...) -> S1; ... Cn(vn,...) -> Sn; }":
 *   1. [[E]] = SumType(TypeName, { C1->payloads..., ..., Cn->... })
 *      where each payload slot is a TopVar keyed to the ASTSumVariant param.
 *   2. For each arm Ci(vi,...): [[vi_j]] = [[variant_param_i_j]]
 */
void TypeConstraintVisitor::endVisit(ASTCaseStmt *element) {
  if (element->getArms().empty())
    return;

  // Identify the owning sum type declaration from the first arm's tag.
  auto firstTag = element->getArms()[0]->getTag();
  auto *ownerDecl = symbolTable->getConstructorOwner(firstTag);
  if (!ownerDecl)
    return; // unknown constructor caught by weeding

  // Build TopSumType: one type var per variant param (keyed to the param node).
  std::vector<std::string> ctorNames;
  std::vector<std::shared_ptr<TopType>> payloads;
  std::map<std::string, int> arities;

  for (auto *variant : ownerDecl->getVariants()) {
    const std::string &tag = variant->getTag();
    ctorNames.push_back(tag);
    auto params = variant->getParams();
    arities[tag] = static_cast<int>(params.size());
    for (auto *param : params) {
      payloads.push_back(astToVar(param));
    }
  }

  auto sumTy = std::make_shared<TopSumType>(ownerDecl->getName(), ctorNames,
                                            payloads, arities);
  constraintHandler->handle(astToVar(element->getCaseExpr()), sumTy);

  // Constrain each arm's patterns against the corresponding variant params.
  for (auto *arm : element->getArms()) {
    auto *variant = symbolTable->getConstructor(arm->getTag());
    if (!variant)
      continue;
    auto variantParams = variant->getParams();
    auto armPatterns   = arm->getPatterns();
    for (std::size_t i = 0;
         i < armPatterns.size() && i < variantParams.size(); ++i) {
      constrainPattern(armPatterns[i], astToVar(variantParams[i]),
                       variantParams[i]);
    }
  }
}

/*! \brief Recursively generate type constraints for a pattern node.
 *
 * \param pat       Pattern to constrain.
 * \param slotType  The TopType that the pattern is matching against.
 *
 * Rules:
 *   - ASTVarPattern(v)              → type(v) = slotType
 *   - ASTWildcardPattern            → (no constraint)
 *   - ASTCtorPattern(tag, subPats)  -> slotType = SumType(owner), then
 *                                     recurse for each sub-pattern
 */
void TypeConstraintVisitor::constrainPattern(ASTPattern *pat,
                                             std::shared_ptr<TopType> slotType,
                                             ASTNode *anchor) {
  if (auto *vp = dynamic_cast<ASTVarPattern *>(pat)) {
    constraintHandler->handle(astToVar(vp->getDecl()), slotType);

  } else if (dynamic_cast<ASTWildcardPattern *>(pat)) {
    // Wildcard — no constraint needed.

  } else if (auto *cp = dynamic_cast<ASTCtorPattern *>(pat)) {
    // Nested constructor pattern: constrain the slot to be the inner sum type,
    // then recurse for each sub-pattern against the inner constructor's params.
    auto *innerOwner = symbolTable->getConstructorOwner(cp->getTag());
    if (!innerOwner)
      return; // unknown ctor; CheckPatternTypes already reported this

    auto *innerVariant = symbolTable->getConstructor(cp->getTag());
    if (!innerVariant)
      return;

    // Build TopSumType for the inner owner.
    std::vector<std::string> ctorNames;
    std::vector<std::shared_ptr<TopType>> payloads;
    std::map<std::string, int> arities;
    for (auto *v : innerOwner->getVariants()) {
      ctorNames.push_back(v->getTag());
      arities[v->getTag()] = static_cast<int>(v->getParams().size());
      for (auto *p : v->getParams())
        payloads.push_back(astToVar(p));
    }
    auto innerSumTy = std::make_shared<TopSumType>(
        innerOwner->getName(), ctorNames, payloads, arities);
    constraintHandler->handle(slotType, innerSumTy);

    // Recurse into sub-patterns.
    auto innerParams = innerVariant->getParams();
    auto &subPats    = cp->getSubPatternsShared();
    for (std::size_t j = 0;
         j < subPats.size() && j < innerParams.size(); ++j) {
      constrainPattern(subPats[j].get(), astToVar(innerParams[j]),
                       innerParams[j]);
    }

  }
}

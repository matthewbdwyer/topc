#pragma once

#include "FunctionEffectSummaries.h"

#include <map>
#include <set>
#include <string>
#include <vector>

class ASTCaseStmt;
class ASTDeclNode;
class ASTDeRefExpr;
class ASTExpr;
class ASTFunction;
class ASTProgram;
class ASTStmt;
class OwnershipClassifier;
class SymbolTable;
class TypeInference;

/*! \class AliasCheck
 *  \brief You cannot move an owned value out of a borrow.
 *
 * An *alias* is an owned value reached through a borrow rather than held by
 * its owner: an Own-typed dereference `*p` of a borrow, or an Own-typed arm
 * binder of `case *p`. Two owners of one allocation free it twice, so an alias
 * may appear only where it is read, not taken:
 *
 *   - under `&`            reborrow it (`height(&l)`)
 *   - under another `*`    read a Copy through it (`**p`)
 *   - as a `case *e` scrutinee
 *   - as an assignment target `*e = v`, where the slot must be Copy
 *
 * Anywhere else -- return, assignment RHS, call actual, constructor payload,
 * by-value `case` scrutinee -- is a move and is rejected here.
 *
 * When the dereferenced type is still a type variable at the definition
 * (`read(p) { return *p; }` has type `(borrow&a) -> a`), the decision belongs
 * to each call site. This pass records a per-formal *requirement* -- the
 * referent of the borrow, or the argument itself, must be Copy -- which
 * FunctionEffectSummaries propagates through pass-through calls and checks
 * against the actual's type wherever it is concrete. This is the same
 * shape as the existing DependsOnInstantiation treatment of consumption.
 */
class AliasCheck {
public:
  using Requirement = FunctionEffectSummaries::CopyRequirement;
  using FormalRequirement = FunctionEffectSummaries::FormalRequirement;
  using Requirements =
      std::map<ASTDeclNode *, std::vector<FormalRequirement>>;

  /*! \brief Check every function; return the per-formal requirements each
   *  generic body imposes on its callers. Throws SemanticError. */
  static Requirements run(ASTProgram *p, SymbolTable *sym, TypeInference *types,
                          OwnershipClassifier *classifier);

private:
  enum class Ctx { Move, Reborrow, Deref, Scrutinee };

  AliasCheck(SymbolTable *sym, TypeInference *types,
             OwnershipClassifier *classifier)
      : sym(sym), types(types), classifier(classifier) {}

  void checkFunction(ASTFunction *f);
  void checkStmt(ASTStmt *stmt);
  void checkExpr(ASTExpr *expr, Ctx ctx);
  void checkDerefTaken(ASTDeRefExpr *deref, const char *what);
  void require(ASTExpr *operand, ASTDeRefExpr *deref, const char *what);

  SymbolTable *sym;
  TypeInference *types;
  OwnershipClassifier *classifier;

  ASTFunction *current = nullptr;
  /// Names of alias binders in scope (the arms being checked).
  std::set<std::string> aliasNames;
  Requirements requirements;
};

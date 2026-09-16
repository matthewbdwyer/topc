#pragma once

#include "ASTDeclNode.h"
#include "ASTProgram.h"
#include "FunctionEffectSummaries.h"
#include "OwnershipClassifier.h"
#include "SymbolTable.h"

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

class ASTCaseArm;
class ASTVariableExpr;

/*!
 * \class MoveAnalysis
 * \brief Forward dataflow analysis tracking ownership state for
 *        every Own-classified variable.
 *
 * Ownership states (per program point):
 * - Owned  — variable holds a live resource.
 * - Moved  — resource has been transferred; variable is invalid.
 *
 * Transfer rules:
 *   x = y   (y : Own, directly)  → y : Moved, x : Owned.
 *   x = e   (e not a direct Own var) → x : Owned (if x : Own).
 *   Use of y where y : Own, y : Moved → use-after-move error.
 *   Assigning over a live Owned Own variable → assign-over-live-own error.
 *   Expressions are walked in evaluation order: a move takes effect where it
 *   happens, so a later use in the same statement sees it.
 *   A call's borrowed owners (`&x` in any actual) may not be moved by any
 *   actual of that call.
 *   if/case join: a variable Owned on one path must be Owned on every path.
 *   while condition and body: may not change which Own variables are Owned.
 *   Owned binders of a by-value case are Owned for the arm and go out of
 *   scope at its end (freed there by DestructionPass if still Owned).
 *
 * \throws SemanticError on any violation.
 */
class MoveAnalysis {
public:
  struct MoveTraceEvent {
    std::string kind;
    std::string variable;
    int line;
    std::string detail;
  };

  enum class OwnershipState { Owned, Moved };
  using StateMap = std::map<ASTDeclNode *, OwnershipState>;

  /*! \brief Run the analysis over every function in \p ast.
   *
   * \throws SemanticError on any ownership violation.
   */
  MoveAnalysis(ASTProgram *ast, SymbolTable *sym, OwnershipClassifier *oc,
               FunctionEffectSummaries *effects);

  /*! \brief Returns retained trace events from the most recent run. */
  static const std::vector<MoveTraceEvent> &getLastTrace();

  /*! \brief Owned binders of \p arm (none for a borrowed scrutinee). */
  static std::vector<ASTDeclNode *>
  ownedBinders(ASTCaseArm *arm, bool byValue, OwnershipClassifier *classifier);

  /*! \brief Join branch states. A variable Owned on some branches but not
   *  all is a disagreement (thrown when \p check); Owned on all stays Owned,
   *  Moved on any other is Moved, otherwise uninitialized (absent). Shared
   *  with DestructionPass so the two passes cannot disagree about a join.
   */
  static StateMap joinStates(const std::vector<StateMap> &branches, bool check);

  /*! \brief A loop body may not change which Own variables are Owned. */
  static void assertLoopInvariant(const StateMap &preState,
                                  const StateMap &bodyState, int line);

private:
  ASTProgram *program;
  SymbolTable *sym;
  OwnershipClassifier *classifier;
  FunctionEffectSummaries *functionEffects;
  ASTDeclNode *currentFuncDecl; ///< set while analysing a function
  std::set<ASTDeclNode *> currentFormals;
  std::vector<MoveTraceEvent> trace;
  /// Borrowed owners of each call whose actuals are being evaluated.
  std::vector<std::pair<ASTFunAppExpr *, std::set<ASTDeclNode *>>> heldBorrows;
  /// Variables moved so far in the current statement (for the message).
  std::set<ASTDeclNode *> movedInStmt;
  static std::vector<MoveTraceEvent> lastTrace;

  void analyzeFunction(ASTFunction *f);

  /*! \brief Execute the transfer function for \p stmt on \p state.
   *  \return Updated state after \p stmt.
   */
  StateMap analyzeStmt(ASTStmt *stmt, StateMap state);

  /*! \brief Specialised transfer for ASTAssignStmt. */
  StateMap analyzeAssign(ASTAssignStmt *stmt, StateMap state);

  /*! \brief Transfer for one case arm; owned binders are arm-scoped. */
  StateMap analyzeArm(ASTCaseArm *arm, bool byValue, StateMap state);

  /*! \brief Evaluate \p node in evaluation order: check each use against the
   *  current state and apply each move where it happens.
   */
  void evalExpr(ASTNode *node, StateMap &state);

  /*! \brief Throw use-after-move if \p varExpr names a Moved Own variable. */
  void checkUse(ASTVariableExpr *varExpr, const StateMap &state) const;

  /*! \brief Move \p decl: reject if already Moved or borrowed by a call in
   *  progress; record the trace event. */
  void consumeVar(ASTVariableExpr *varExpr, ASTDeclNode *decl, StateMap &state,
                  const char *reason);

  /*! \brief Owners borrowed (`&x`) anywhere inside \p node. */
  void collectBorrowedOwners(ASTNode *node,
                             std::set<ASTDeclNode *> &owners) const;

  /*! \brief Resolve a variable name to its ASTDeclNode in the current function,
   *         falling back to global function names.
   *  \return nullptr if the name is not found.
   */
  ASTDeclNode *resolveVar(const std::string &name) const;


};

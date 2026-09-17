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
class ASTCaseStmt;
class ASTDeRefExpr;
class ASTPattern;
class ASTFunction;
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
 * The same walk records where destruction is needed (destructionPlan()):
 * owners still Owned at a function's exit, and owned binders still Owned at
 * the end of their arm. DestructionPass inserts the destroys.
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

  /*! \brief Where destruction must be inserted, computed by the same walk
   *  that checks the program, so checking and destruction cannot disagree.
   *  DestructionPass applies it. */
  struct DestructionPlan {
    /// Owners still Owned at each function's exit, sorted by name.
    std::vector<std::pair<ASTFunction *, std::vector<ASTDeclNode *>>> atExit;
    /// Owned binders of a by-value case still Owned at the end of each arm.
    std::vector<std::pair<ASTCaseArm *, std::vector<ASTDeclNode *>>> atArmEnd;
    /// `*e` where e is an owning reference made by a call or `alloc` and never
    /// bound: code generation frees it right after the read or write.
    std::set<const ASTDeRefExpr *> freeAfterUse;
    /// By-value matches: the match consumes the scrutinee, so the arm taken
    /// frees nested boxes its patterns match and the box itself after the arm.
    std::set<const ASTCaseStmt *> consumingMatches;
    /// Owned payloads a consuming match discards with `_`, destroyed when the
    /// arm is taken (never while its patterns are still being tested).
    std::set<const ASTPattern *> discardedPayloads;
  };

  /*! \brief Run the analysis over every function in \p ast.
   *
   * \throws SemanticError on any ownership violation.
   */
  MoveAnalysis(ASTProgram *ast, SymbolTable *sym, OwnershipClassifier *oc,
               FunctionEffectSummaries *effects);

  /*! \brief Returns retained trace events from the most recent run. */
  static const std::vector<MoveTraceEvent> &getLastTrace();

  /*! \brief Where the checked program needs destruction. */
  const DestructionPlan &destructionPlan() const { return plan; }

private:
  SymbolTable *sym;
  OwnershipClassifier *classifier;
  FunctionEffectSummaries *functionEffects;
  ASTDeclNode *currentFuncDecl = nullptr; ///< set while analysing a function
  std::vector<MoveTraceEvent> trace;
  DestructionPlan plan;
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
                  const char *reason, const char *logReason);

  /*! \brief Owned binders of \p arm (none for a borrowed scrutinee). */
  std::vector<ASTDeclNode *> ownedBinders(ASTCaseArm *arm, bool byValue) const;

  /*! \brief Join branch states. A variable Owned on some branches but not
   *  all is a disagreement; Owned on all stays Owned, Moved on any other is
   *  Moved, otherwise uninitialized (absent). */
  static StateMap joinStates(const std::vector<StateMap> &branches);

  /*! \brief A loop body may not change which Own variables are Owned. */
  static void assertLoopInvariant(const StateMap &preState,
                                  const StateMap &bodyState, int line);

  /*! \brief Record the owned payloads \p pat discards with `_` in a
   *  consuming match; \p payload is the constructor parameter it matches. */
  void collectDiscardedPayloads(ASTPattern *pat, ASTDeclNode *payload);

  /*! \brief Record `*e` for freeing after use if e is an unbound owning
   *  reference (a call or alloc). */
  void noteUnboundReference(ASTDeRefExpr *deref);

  /*! \brief Owners borrowed (`&x`) anywhere inside \p node. */
  void collectBorrowedOwners(ASTNode *node,
                             std::set<ASTDeclNode *> &owners) const;

  /*! \brief Resolve a variable name to its ASTDeclNode in the current function,
   *         falling back to global function names.
   *  \return nullptr if the name is not found.
   */
  ASTDeclNode *resolveVar(const std::string &name) const;


};

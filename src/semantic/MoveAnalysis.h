#pragma once

#include "ASTDeclNode.h"
#include "ASTProgram.h"
#include "OwnershipClassifier.h"
#include "SymbolTable.h"

#include <map>
#include <string>
#include <vector>

/*!
 * \class MoveAnalysis
 * \brief Phase 9 pass: forward dataflow analysis tracking ownership state for
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
 *   if/case join: both paths must leave every Own variable in the same state.
 *   while/for body: may not change the state of any Own variable.
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
  MoveAnalysis(ASTProgram *ast, SymbolTable *sym, OwnershipClassifier *oc);

  /*! \brief Returns retained trace events from the most recent run. */
  static const std::vector<MoveTraceEvent> &getLastTrace();

private:
  SymbolTable *sym;
  OwnershipClassifier *classifier;
  ASTDeclNode *currentFuncDecl; ///< set while analysing a function
  std::vector<MoveTraceEvent> trace;
  static std::vector<MoveTraceEvent> lastTrace;

  void analyzeFunction(ASTFunction *f);

  /*! \brief Execute the transfer function for \p stmt on \p state.
   *  \return Updated state after \p stmt.
   */
  StateMap analyzeStmt(ASTStmt *stmt, StateMap state);

  /*! \brief Specialised transfer for ASTAssignStmt. */
  StateMap analyzeAssign(ASTAssignStmt *stmt, StateMap state);

  /*! \brief Recursively verify that no Moved Own variable appears in \p expr.
   *
   * Does NOT update the state — ownership transfers happen only at the
   * statement level.
   */
  void checkExprForMoved(ASTNode *node, const StateMap &state) const;

  /*! \brief Resolve a variable name to its ASTDeclNode in the current function,
   *         falling back to global function names.
   *  \return nullptr if the name is not found.
   */
  ASTDeclNode *resolveVar(const std::string &name) const;

  /*! \brief Verify that \p thenState and \p elseState agree on every Own var.
   *
   * Iterates over the union of keys from both maps and \p preState; if any
   * Own variable differs between the two branch states, throws SemanticError.
   */
  static void assertJoinAgrees(const StateMap &preState,
                                const StateMap &thenState,
                                const StateMap &elseState);
};

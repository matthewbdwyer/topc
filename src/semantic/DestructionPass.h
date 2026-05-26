#pragma once

#include "ASTDeclNode.h"
#include "ASTProgram.h"
#include "MoveAnalysis.h"
#include "OwnershipClassifier.h"
#include "SymbolTable.h"

#include <map>
#include <string>

class ASTAssignStmt;
class ASTBlockStmt;
class ASTFunction;
class ASTIfStmt;
class ASTReturnStmt;
class ASTStmt;
class ASTWhileStmt;

/*!
 * \class DestructionPass
 * \brief Phase 11 pass: insert ASTDestroyStmt nodes before function returns.
 *
 * Re-runs a simplified forward ownership dataflow (without error checking —
 * the program has already been validated by MoveAnalysis) to determine which
 * Own variables are still Owned at each function's return point, then inserts
 * ASTDestroyStmt nodes for each such variable before the return statement.
 */
class DestructionPass {
public:
  using OwnershipState = MoveAnalysis::OwnershipState;
  using StateMap       = MoveAnalysis::StateMap;

  /*! \brief Run the pass over every function in \p ast. */
  static void run(ASTProgram *ast, SymbolTable *sym, OwnershipClassifier *oc);

private:
  SymbolTable *sym;
  OwnershipClassifier *classifier;
  ASTDeclNode *currentFuncDecl = nullptr;

  DestructionPass(SymbolTable *sym, OwnershipClassifier *oc);

  void      processFunction(ASTFunction *f);
  StateMap  analyzeStmt(ASTStmt *stmt, StateMap state);
  StateMap  analyzeAssign(ASTAssignStmt *stmt, StateMap state);

  /*! \brief Resolve a variable name to its ASTDeclNode in the current scope. */
  ASTDeclNode *resolveVar(const std::string &name) const;
};

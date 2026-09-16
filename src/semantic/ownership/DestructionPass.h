#pragma once

#include "MoveAnalysis.h"

class ASTProgram;

/*!
 * \class DestructionPass
 * \brief Insert ASTDestroyStmt nodes where MoveAnalysis found owners still
 *        Owned: before each function's return, and at the end of each case
 *        arm whose owned binders were not moved on.
 *
 * The decisions come from MoveAnalysis::destructionPlan(), computed by the
 * walk that checked the program, so there is no second ownership walk to
 * disagree with the first. This pass only rewrites the AST.
 */
class DestructionPass {
public:
  /*! \brief Insert the destroys \p plan calls for. */
  static void run(ASTProgram *ast, const MoveAnalysis::DestructionPlan &plan);
};

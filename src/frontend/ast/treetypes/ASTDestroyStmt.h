#pragma once

#include "ASTDeclNode.h"
#include "ASTStmt.h"

/*!
 * \brief Synthetic statement inserted by the Destruction Pass.
 *
 * Represents the deallocation of the heap resource owned by \p VAR.
 * This node is never produced by the parser; it is injected into the
 * function body by DestructionPass after semantic analysis.
 *
 * It is a leaf node — accept() calls visit/endVisit but does not
 * recurse into any children, preventing standard traversals from
 * re-processing the referenced declaration.
 */
class ASTDestroyStmt : public ASTStmt {
  ASTDeclNode *VAR; // non-owning — lifetime managed by the AST tree

public:
  explicit ASTDestroyStmt(ASTDeclNode *var) : VAR(var) {}
  ASTDeclNode *getVar() const { return VAR; }
  void accept(ASTVisitor *visitor) override;
  std::vector<std::shared_ptr<ASTNode>> getChildren() override;

protected:
  std::ostream &print(std::ostream &out) const override;
};

#pragma once

#include "ASTDeclNode.h"
#include "ASTExpr.h"
#include "ASTStmt.h"

/*! \brief Class for a for-loop (SOP stub — iteration semantics in sopc).
 *
 * Example: `for (x : s) output x;`
 */
class ASTForStmt : public ASTStmt {
  std::shared_ptr<ASTDeclNode> VAR;
  std::shared_ptr<ASTExpr> ITERABLE;
  std::shared_ptr<ASTStmt> BODY;

public:
  std::vector<std::shared_ptr<ASTNode>> getChildren() override;
  ASTForStmt(std::shared_ptr<ASTDeclNode> var,
             std::shared_ptr<ASTExpr> iterable,
             std::shared_ptr<ASTStmt> body)
      : VAR(std::move(var)), ITERABLE(std::move(iterable)),
        BODY(std::move(body)) {}
  ASTDeclNode *getVar() const { return VAR.get(); }
  ASTExpr *getIterable() const { return ITERABLE.get(); }
  ASTStmt *getBody() const { return BODY.get(); }
  void accept(ASTVisitor *visitor) override;

protected:
  std::ostream &print(std::ostream &out) const override;
};

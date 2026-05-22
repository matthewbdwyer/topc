#pragma once

#include "ASTDeclNode.h"
#include "ASTStmt.h"

#include <string>
#include <vector>

/*! \brief Class for a single arm in a case statement.
 *
 * Example: `Some(v) -> output v;`
 */
class ASTCaseArm : public ASTNode {
  std::string TAG;
  std::vector<std::shared_ptr<ASTDeclNode>> BINDINGS;
  std::shared_ptr<ASTStmt> BODY;

public:
  std::vector<std::shared_ptr<ASTNode>> getChildren() override;
  ASTCaseArm(std::string tag,
             std::vector<std::shared_ptr<ASTDeclNode>> bindings,
             std::shared_ptr<ASTStmt> body)
      : TAG(std::move(tag)), BINDINGS(std::move(bindings)),
        BODY(std::move(body)) {}
  const std::string &getTag() const { return TAG; }
  std::vector<ASTDeclNode *> getBindings() const;
  ASTStmt *getBody() const { return BODY.get(); }
  void accept(ASTVisitor *visitor) override;

protected:
  std::ostream &print(std::ostream &out) const override;
};

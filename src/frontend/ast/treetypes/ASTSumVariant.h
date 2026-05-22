#pragma once

#include "ASTDeclNode.h"

#include <string>
#include <vector>

/*! \brief Class for a constructor variant in a sum type declaration.
 *
 * Example: `Some(x)` or `None` in `type Option = Some(x) | None;`
 */
class ASTSumVariant : public ASTNode {
  std::string TAG;
  std::vector<std::shared_ptr<ASTDeclNode>> PARAMS;

public:
  std::vector<std::shared_ptr<ASTNode>> getChildren() override;
  ASTSumVariant(std::string tag,
                std::vector<std::shared_ptr<ASTDeclNode>> params)
      : TAG(std::move(tag)), PARAMS(std::move(params)) {}
  const std::string &getTag() const { return TAG; }
  std::vector<ASTDeclNode *> getParams() const;
  void accept(ASTVisitor *visitor) override;

protected:
  std::ostream &print(std::ostream &out) const override;
};

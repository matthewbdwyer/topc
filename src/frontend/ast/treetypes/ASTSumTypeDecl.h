#pragma once

#include "ASTNode.h"
#include "ASTSumVariant.h"

#include <string>
#include <vector>

/*! \brief Class for a top-level sum type declaration.
 *
 * Example: `type Option = Some(x) | None;`
 */
class ASTSumTypeDecl : public ASTNode {
  std::string NAME;
  std::vector<std::shared_ptr<ASTSumVariant>> VARIANTS;

public:
  std::vector<std::shared_ptr<ASTNode>> getChildren() override;
  ASTSumTypeDecl(std::string name,
                 std::vector<std::shared_ptr<ASTSumVariant>> variants)
      : NAME(std::move(name)), VARIANTS(std::move(variants)) {}
  const std::string &getName() const { return NAME; }
  std::vector<ASTSumVariant *> getVariants() const;
  void accept(ASTVisitor *visitor) override;

protected:
  std::ostream &print(std::ostream &out) const override;
};

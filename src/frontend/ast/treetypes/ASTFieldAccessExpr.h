#pragma once

#include "ASTExpr.h"

#include <memory>
#include <string>

/*! \brief Class for record field access expressions.
 *
 * Example: `r.field`
 */
class ASTFieldAccessExpr : public ASTExpr {
  std::shared_ptr<ASTExpr> BASE;
  std::string FIELD;

public:
  ASTFieldAccessExpr(std::shared_ptr<ASTExpr> base, std::string field)
      : BASE(std::move(base)), FIELD(std::move(field)) {}

  std::vector<std::shared_ptr<ASTNode>> getChildren() override;
  ASTExpr *getBase() const { return BASE.get(); }
  const std::string &getField() const { return FIELD; }
  void accept(ASTVisitor *visitor) override;

protected:
  std::ostream &print(std::ostream &out) const override;
};

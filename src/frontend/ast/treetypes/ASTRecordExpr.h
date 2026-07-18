#pragma once

#include "ASTExpr.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

/*! \brief Class for record construction expressions.
 *
 * Example: `{a:1, b:x}`
 */
class ASTRecordExpr : public ASTExpr {
  std::vector<std::pair<std::string, std::shared_ptr<ASTExpr>>> FIELDS;

public:
  ASTRecordExpr(std::vector<std::pair<std::string, std::shared_ptr<ASTExpr>>> fields)
      : FIELDS(std::move(fields)) {}

  std::vector<std::shared_ptr<ASTNode>> getChildren() override;
  std::vector<std::string> getFieldNames() const;
  std::vector<ASTExpr *> getFieldValues() const;
  void accept(ASTVisitor *visitor) override;

protected:
  std::ostream &print(std::ostream &out) const override;
};

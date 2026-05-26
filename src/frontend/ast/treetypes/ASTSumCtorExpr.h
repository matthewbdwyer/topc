#pragma once

#include "ASTExpr.h"

#include <memory>
#include <string>
#include <vector>

/*! \brief Class for sum type constructor expressions.
 *
 * Examples: `None`, `Some(42)`, `Circle(r)`, `Node(v, l, r)`
 *
 * Nullary constructors (no payload) have an empty ARGS vector.
 */
class ASTSumCtorExpr : public ASTExpr {
  std::string TAG;
  std::vector<std::shared_ptr<ASTExpr>> ARGS;

public:
  std::vector<std::shared_ptr<ASTNode>> getChildren() override;
  ASTSumCtorExpr(std::string tag, std::vector<std::shared_ptr<ASTExpr>> args)
      : TAG(std::move(tag)), ARGS(std::move(args)) {}
  const std::string &getTag() const { return TAG; }
  std::vector<ASTExpr *> getArgs() const;
  void accept(ASTVisitor *visitor) override;

protected:
  std::ostream &print(std::ostream &out) const override;
};

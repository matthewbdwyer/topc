#pragma once

#include "ASTExpr.h"
#include <stdexcept>

/*! \brief Class for a binary operator.
 */
class ASTBinaryExpr : public ASTExpr {
public:
  enum class BinaryOp { Add, Sub, Mul, Div, Gt, Eq, Neq };

private:
  std::string OP;
  BinaryOp KIND;
  std::shared_ptr<ASTExpr> LEFT, RIGHT;

  static BinaryOp parseOp(const std::string &op);

public:
  std::vector<std::shared_ptr<ASTNode>> getChildren() override;
  ASTBinaryExpr(const std::string &OP, std::shared_ptr<ASTExpr> LEFT,
                std::shared_ptr<ASTExpr> RIGHT)
      : OP(OP), KIND(parseOp(OP)), LEFT(LEFT), RIGHT(RIGHT) {}
  const std::string &getOp() const { return OP; }
  BinaryOp getOpKind() const { return KIND; }
  ASTExpr *getLeft() const { return LEFT.get(); }
  ASTExpr *getRight() const { return RIGHT.get(); }
  void accept(ASTVisitor *visitor) override;
  llvm::Value *codegen() override;

protected:
  std::ostream &print(std::ostream &out) const override;
};

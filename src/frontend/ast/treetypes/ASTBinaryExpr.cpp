#include "ASTBinaryExpr.h"
#include "ASTVisitor.h"
#include "InternalError.h"

ASTBinaryExpr::BinaryOp ASTBinaryExpr::parseOp(const std::string &op) {
  if (op == "+")  return BinaryOp::Add;
  if (op == "-")  return BinaryOp::Sub;
  if (op == "*")  return BinaryOp::Mul;
  if (op == "/")  return BinaryOp::Div;
  if (op == ">")  return BinaryOp::Gt;
  if (op == "==") return BinaryOp::Eq;
  if (op == "!=") return BinaryOp::Neq;
  throw InternalError("Unknown binary operator: " + op);
}

void ASTBinaryExpr::accept(ASTVisitor *visitor) {
  if (visitor->visit(this)) {
    getLeft()->accept(visitor);
    getRight()->accept(visitor);
  }
  visitor->endVisit(this);
}

std::ostream &ASTBinaryExpr::print(std::ostream &out) const {
  out << "(" << *getLeft() << getOp() << *getRight() << ")";
  return out;
} // LCOV_EXCL_LINE

std::vector<std::shared_ptr<ASTNode>> ASTBinaryExpr::getChildren() {
  std::vector<std::shared_ptr<ASTNode>> children;
  children.push_back(LEFT);
  children.push_back(RIGHT);
  return children;
}

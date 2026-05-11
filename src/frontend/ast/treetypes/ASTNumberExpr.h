#pragma once

#include "ASTExpr.h"

/*! \brief Class for numeric literals.
 */
class ASTNumberExpr : public ASTExpr {
  int VAL;

public:
  ASTNumberExpr(int VAL) : VAL(VAL) {}
  int getValue() const { return VAL; }
  void accept(ASTVisitor *visitor) override;

protected:
  std::ostream &print(std::ostream &out) const override;
};

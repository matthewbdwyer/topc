#pragma once

#include "ASTExpr.h"

#include <cstdint>

/*! \brief Class for numeric literals.
 */
class ASTNumberExpr : public ASTExpr {
  int64_t VAL;

public:
  ASTNumberExpr(int64_t VAL) : VAL(VAL) {}
  int64_t getValue() const { return VAL; }
  void accept(ASTVisitor *visitor) override;

protected:
  std::ostream &print(std::ostream &out) const override;
};

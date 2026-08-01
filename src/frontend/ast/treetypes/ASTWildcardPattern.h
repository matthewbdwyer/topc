#pragma once

#include "ASTPattern.h"

/*! \brief Wildcard pattern — matches anything and binds nothing.
 *
 * Example: the `_` in `Some(_)`.
 */
class ASTWildcardPattern : public ASTPattern {
public:
  std::ostream &print(std::ostream &out) const override {
    return out << "_";
  }
};

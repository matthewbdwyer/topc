#pragma once

#include <ostream>

class ASTVisitor;

/*! \brief Abstract base class for pattern nodes in case arms.
 *
 * Patterns appear in the payload of case arms, e.g.:
 *   `Some(x)`         — x is an ASTVarPattern
 *   `Some(_)`         — _ is an ASTWildcardPattern
 *   `Some(Inner(x))`  — Inner(x) is an ASTCtorPattern
 */
class ASTPattern {
public:
  virtual ~ASTPattern() = default;
  virtual std::ostream &print(std::ostream &out) const = 0;
  friend std::ostream &operator<<(std::ostream &os, const ASTPattern &p) {
    return p.print(os);
  }
};

#include "ASTCtorPattern.h"

std::ostream &ASTCtorPattern::print(std::ostream &out) const {
  out << TAG;
  if (!SUB_PATTERNS.empty()) {
    out << "(";
    bool first = true;
    for (auto &s : SUB_PATTERNS) {
      if (!first)
        out << ", ";
      out << *s;
      first = false;
    }
    out << ")";
  }
  return out;
}

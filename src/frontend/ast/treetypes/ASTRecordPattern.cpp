#include "ASTRecordPattern.h"

std::ostream &ASTRecordPattern::print(std::ostream &out) const {
  out << "{";
  bool first = true;
  for (auto &[field, pat] : FIELDS) {
    if (!first)
      out << ", ";
    out << field << ": " << *pat;
    first = false;
  }
  out << "}";
  return out;
}

#include "Termination.h"

#include "AST.h"

namespace verutop {

std::string cycleObligation(ASTFunction *from, ASTFunction *to, bool &both) {
  both = from->getDecreases() && to->getDecreases();
  if (from == to)
    return both ? "recursive call decreases the measure" : "recursive call needs a decreases clause";
  if (both) return "mutually recursive call decreases the measure";
  std::string missing;
  for (auto *f : {from, to})
    if (!f->getDecreases()) missing += (missing.empty() ? "" : " and ") + f->getName();
  return "mutually recursive call needs a decreases clause on " + missing;
}

} // namespace verutop

#include "Termination.h"

#include "AST.h"

namespace verutop {

Obligation cycleObligation(ASTFunction *from, ASTFunction *to, ASTNode *call) {
  Obligation ob;
  ob.line = call->getLine();
  ob.column = call->getColumn();
  ob.callee = to->getName();
  bool both = from->getDecreases() && to->getDecreases();
  if (from == to) {
    ob.kind = both ? ObKind::CallMeasure : ObKind::MissingCallMeasure;
    return ob;
  }
  ob.kind = both ? ObKind::MutualCallMeasure : ObKind::MissingMutualCallMeasure;
  if (!both)
    for (auto *f : {from, to})
      if (!f->getDecreases()) ob.subject += (ob.subject.empty() ? "" : " and ") + f->getName();
  return ob;
}

} // namespace verutop

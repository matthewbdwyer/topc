#include "TypeVars.h"

TopVarSet TypeVars::collect(TopType *t) {
  TypeVars visitor;
  t->accept(&visitor);
  return visitor.getVars();
}

void TypeVars::endVisit(TopMu *element) { vars.erase(element->getV()); }

void TypeVars::endVisit(TopVar *element) {
  vars.insert(std::make_shared<TopVar>(element->getNode()));
}

void TypeVars::endVisit(TopAlpha *element) {
  vars.insert(
      std::make_shared<TopAlpha>(element->getNode(), element->getContext(), element->getName()));
}

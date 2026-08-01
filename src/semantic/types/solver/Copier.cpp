#include "Copier.h"

/*
 * The Copier inherits all of the methods above from Substituter, but
 * it overrides the behavior for TopVar and TopAlpha.
 */
std::shared_ptr<TopType> Copier::copy(std::shared_ptr<TopType> t) {
  Copier visitor;
  t->accept(&visitor);
  return visitor.getResult();
}

void Copier::endVisit(TopVar *element) {
  visitedTypes.push_back(std::make_shared<TopVar>(element->getNode()));
}

void Copier::endVisit(TopAlpha *element) {
  visitedTypes.push_back(
      std::make_shared<TopAlpha>(element->getNode(), element->getName()));
}

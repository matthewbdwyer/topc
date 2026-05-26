#include "FreshAlphaCopier.h"

/*
 * The Copier inherits all of the methods above from Substituter, but
 * it overrides the behavior for TopVar and TopAlpha.
 */
std::shared_ptr<TopType> FreshAlphaCopier::copy(TopType *t, ASTNode *c) {
  FreshAlphaCopier visitor;
  visitor.context = c;
  t->accept(&visitor);
  return visitor.getResult();
}

void FreshAlphaCopier::endVisit(TopAlpha *element) {
  visitedTypes.push_back(std::make_shared<TopAlpha>(element->getNode(), context,
                                                    element->getName()));
}

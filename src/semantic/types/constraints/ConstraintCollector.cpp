#include "ConstraintCollector.h"

#include "../../SemanticLogging.h"

#include <iostream>

void ConstraintCollector::handle(std::shared_ptr<TopType> t1,
                                 std::shared_ptr<TopType> t2) {
  SEMANTIC_LOG(3, "type-constraints")
      << "generate lhs=" << *t1 << " rhs=" << *t2;
  collected.emplace_back(t1, t2);
}

std::vector<TypeConstraint> &ConstraintCollector::getCollectedConstraints() {
  return collected;
}

#include "ConstraintCollector.h"

#include "loguru.hpp"

#include <iostream>

void ConstraintCollector::handle(std::shared_ptr<TopType> t1,
                                 std::shared_ptr<TopType> t2) {
  LOG_S(1) << "Generating type constraint: " << *t1 << " = " << *t2;
  collected.emplace_back(t1, t2);
}

std::vector<TypeConstraint> &ConstraintCollector::getCollectedConstraints() {
  return collected;
}

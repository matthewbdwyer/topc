#include "ConstraintUnifier.h"

void ConstraintUnifier::handle(std::shared_ptr<TopType> t1,
                               std::shared_ptr<TopType> t2) {
  unifier.unify(t1, t2);
}

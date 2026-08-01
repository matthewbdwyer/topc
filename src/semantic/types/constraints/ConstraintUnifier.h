#pragma once

#include "ConstraintHandler.h"
#include "Unifier.h"
#include "TypeConstraint.h"

/*!
 * \class ConstraintUnifier
 *
 * \brief A constraint handler to unify constraints on the fly.
 */
class ConstraintUnifier : public ConstraintHandler {
public:
  void handle(std::shared_ptr<TopType> t1,
              std::shared_ptr<TopType> t2) override;

private:
  Unifier unifier;
};

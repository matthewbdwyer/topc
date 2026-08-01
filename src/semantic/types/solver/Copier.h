#pragma once

#include "Substituter.h"

/*! \brief Makes a copy of a TopType
 *
 * This subtype of the Substituter overrides the behavior for TopVar
 * and TopAlpha to just copy that node rather than perform a substitution.
 */
class Copier : public Substituter {
public:
  Copier() = default;

  static std::shared_ptr<TopType> copy(std::shared_ptr<TopType> s);

  virtual void endVisit(TopAlpha *element) override;
  virtual void endVisit(TopVar *element) override;
};

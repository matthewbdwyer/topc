#pragma once

#include "TopTypeVisitor.h"
#include "TopVar.h"
#include <set>

/*! \brief Produces set of type variables in a type expression.
 */
class TypeVars : public TopTypeVisitor {
  TopVarSet vars;

public:
  TypeVars() = default;

  /*! \brief Collect the set of type variables in a type expression.
   *
   * \param t The type within which to collect variables.
   * \return The set of type variables.
   */
  static TopVarSet collect(TopType *t);

  TopVarSet getVars() { return vars; }

  virtual void endVisit(TopMu *element) override;
  virtual void endVisit(TopAlpha *element) override;
  virtual void endVisit(TopVar *element) override;
};

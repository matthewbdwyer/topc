#pragma once

#include "TipTypeVisitor.h"
#include "TipVar.h"
#include <set>

/*! \brief Produces set of type variables in a type expression.
 */
class TypeVars : public TipTypeVisitor {
  TipVarSet vars;

public:
  TypeVars() = default;

  /*! \brief Collect the set of type variables in a type expression.
   *
   * \param t The type within which to collect variables.
   * \return The set of type variables.
   */
  static TipVarSet collect(TipType *t);

  TipVarSet getVars() { return vars; }

  virtual void endVisit(TipMu *element) override;
  virtual void endVisit(TipAlpha *element) override;
  virtual void endVisit(TipVar *element) override;
};

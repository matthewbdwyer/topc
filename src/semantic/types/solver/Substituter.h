#pragma once

#include "TopTypeVisitor.h"

/*! \brief Produces a type with designated variable substitutions.
 */
class Substituter : public TopTypeVisitor {
  TopVar *target;
  std::shared_ptr<TopType> substitution;

protected:
  std::vector<std::shared_ptr<TopType>> visitedTypes;
  Substituter() = default;

public:
  Substituter(TopVar *t, std::shared_ptr<TopType> s)
      : target(t), substitution(s) {}

  /*! \brief Substitute for instances of variable in a target type.
   *
   * This function requires the substitution to be a shared_ptr so that it can
   * be directly substituted for the variable without having to be
   * reconstructed. This simplifies things when the substitution is a complex
   * type expression. It does lead to a bit of asymmetry in the API and it will
   * lead to sharing among type expressions, which is why we use shared_ptrs.
   *
   * \param t The type on which substitution is performed.
   * \param v The target variable.
   * \param s The subtitution.
   * \return An equivalent type with no occurrences of the target variable.
   */
  static std::shared_ptr<TopType> substitute(TopType *t, TopVar *v,
                                             std::shared_ptr<TopType> s);

  std::shared_ptr<TopType> getResult();

  virtual void endVisit(TopAlpha *element) override;
  virtual void endVisit(TopAbsentField *element) override;
  virtual void endVisit(TopFunction *element) override;
  virtual void endVisit(TopInt *element) override;
  virtual void endVisit(TopMu *element) override;
  virtual void endVisit(TopRecord *element) override;
  virtual void endVisit(TopRef *element) override;
  virtual void endVisit(TopOwningRef *element) override;
  virtual void endVisit(TopBorrowRef *element) override;
  virtual void endVisit(TopSumType *element) override;
  virtual void endVisit(TopVar *element) override;
};

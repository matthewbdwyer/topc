#pragma once

#include "TipCons.h"
#include <string>
#include <vector>
#include <memory>

class TipTypeVisitor;

/*!
 * \class TipAbsentField
 *
 * \brief A proper type representing an absent record field.
 */
class TipAbsentField : public TipCons {
public:
  TipAbsentField();

  bool operator==(const TipType &other) const override;
  bool operator!=(const TipType &other) const;

  void accept(TipTypeVisitor *visitor) override;

  // ========== Term interface ==========
  std::string getFunctor() const override { return "absent"; }
  std::size_t arity() const override { return 0; }
  std::vector<std::shared_ptr<Term>> getSubterms() const override { return {}; }
  std::shared_ptr<Term> withSubterms(std::vector<std::shared_ptr<Term>> newSubterms) const override;

protected:
  std::ostream &print(std::ostream &out) const override;
};

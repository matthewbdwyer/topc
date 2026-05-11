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

  // ========== Solver-facing interface ==========
  std::string getFunctor() const override { return "absent"; }
  std::size_t arity() const override { return 0; }

  // ========== TipType structural interface ==========
  std::shared_ptr<TipType> withChildTypes(
      std::vector<std::shared_ptr<TipType>> children) const override;

protected:
  std::ostream &print(std::ostream &out) const override;
};

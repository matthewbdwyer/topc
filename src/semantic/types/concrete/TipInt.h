#pragma once

#include "TipCons.h"
#include <string>
#include <vector>
#include <memory>

class TipTypeVisitor;

/*!
 * \class TipInt
 *
 * \brief A proper type representing an int
 */
class TipInt : public TipCons {
public:
  TipInt();

  bool operator==(const TipType &other) const override;
  bool operator!=(const TipType &other) const;

  void accept(TipTypeVisitor *visitor) override;

  // ========== Solver-facing interface ==========
  std::string getFunctor() const override { return "int"; }
  std::size_t arity() const override { return 0; }

  // ========== TipType structural interface ==========
  std::shared_ptr<TipType> withChildTypes(
      std::vector<std::shared_ptr<TipType>> children) const override;

protected:
  std::ostream &print(std::ostream &out) const override;
};

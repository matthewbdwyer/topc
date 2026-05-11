#pragma once

#include "TipCons.h"
#include <memory>
#include <string>
#include <vector>

class TipTypeVisitor;

/*!
 * \class TipRef
 *
 * \brief A proper type representing a pointer type.
 */
class TipRef : public TipCons {
public:
  TipRef() = delete;
  TipRef(std::shared_ptr<TipType> of);

  std::shared_ptr<TipType> getReferencedType() const;

  bool operator==(const TipType &other) const override;
  bool operator!=(const TipType &other) const;

  void accept(TipTypeVisitor *visitor) override;

  // ========== Solver-facing interface ==========
  std::string getFunctor() const override { return "ptr"; }
  std::size_t arity() const override { return 1; }

  // ========== TipType structural interface ==========
  std::shared_ptr<TipType> withChildTypes(
      std::vector<std::shared_ptr<TipType>> children) const override;

protected:
  std::ostream &print(std::ostream &out) const override;
};

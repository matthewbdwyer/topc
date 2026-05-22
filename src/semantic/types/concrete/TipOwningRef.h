#pragma once

#include "TipCons.h"
#include <memory>
#include <string>
#include <vector>

class TipTypeVisitor;

/*!
 * \class TipOwningRef
 *
 * \brief A proper type representing an owning (heap-allocated) pointer type.
 *
 * In TOP programs, `alloc E` produces a `TipOwningRef` rather than a `TipRef`.
 */
class TipOwningRef : public TipCons {
public:
  TipOwningRef() = delete;
  TipOwningRef(std::shared_ptr<TipType> of);

  std::shared_ptr<TipType> getReferencedType() const;

  bool operator==(const TipType &other) const override;
  bool operator!=(const TipType &other) const;

  void accept(TipTypeVisitor *visitor) override;

  // ========== Solver-facing interface ==========
  std::string getFunctor() const override { return "ownref"; }
  std::size_t arity() const override { return 1; }

  // ========== TipType structural interface ==========
  std::shared_ptr<TipType> withChildTypes(
      std::vector<std::shared_ptr<TipType>> children) const override;

protected:
  std::ostream &print(std::ostream &out) const override;
};

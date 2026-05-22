#pragma once

#include "TipCons.h"
#include <memory>
#include <string>
#include <vector>

class TipTypeVisitor;

/*!
 * \class TipBorrowRef
 *
 * \brief A proper type representing a borrow (non-owning reference) pointer type.
 *
 * In TOP programs, `&x` produces a `TipBorrowRef` rather than a `TipRef`.
 */
class TipBorrowRef : public TipCons {
public:
  TipBorrowRef() = delete;
  TipBorrowRef(std::shared_ptr<TipType> of);

  std::shared_ptr<TipType> getReferencedType() const;

  bool operator==(const TipType &other) const override;
  bool operator!=(const TipType &other) const;

  void accept(TipTypeVisitor *visitor) override;

  // ========== Solver-facing interface ==========
  std::string getFunctor() const override { return "borrowref"; }
  std::size_t arity() const override { return 1; }

  // ========== TipType structural interface ==========
  std::shared_ptr<TipType> withChildTypes(
      std::vector<std::shared_ptr<TipType>> children) const override;

protected:
  std::ostream &print(std::ostream &out) const override;
};

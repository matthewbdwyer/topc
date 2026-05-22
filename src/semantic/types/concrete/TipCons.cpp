#include "TipCons.h"
#include "TipBorrowRef.h"
#include "TipOwningRef.h"
#include "TipRecord.h"
#include "TipSumType.h"
#include "TipTypeVisitor.h"

std::size_t TipCons::arity() const { return arguments.size(); }

namespace {
/*! \brief Determine that subtypes of two objects are the same
 */
template <typename T> bool sameType(const TipType *x, const TipType *y) {
  return dynamic_cast<T const *>(x) && dynamic_cast<T const *>(y);
}
} // namespace

/*! \brief Check for dynamic subtype and artity agreement
 * We explicitly test the types here which is not robust to
 * the addition of new subtypes of TipCons.  Extend this if you
 * add such a subtype.
 */
bool TipCons::doMatch(TipType const *t) const {
  // Check if they are both the same TipType subtype
  if (sameType<TipFunction>(t, this) || sameType<TipInt>(t, this) ||
      sameType<TipRef>(t, this) || sameType<TipOwningRef>(t, this) ||
      sameType<TipBorrowRef>(t, this)) {
    auto tipCons = dynamic_cast<TipCons const *>(t);
    return tipCons->arity() == arity();
  }
  if (sameType<TipRecord>(t, this)) {
    auto rec1 = dynamic_cast<TipRecord const *>(this);
    auto rec2 = dynamic_cast<TipRecord const *>(t);
    if (rec1->arity() != rec2->arity()) return false;
    // Two records match only if they share the same canonical field names.
    return rec1->getFunctor() == rec2->getFunctor();
  }
  if (sameType<TipSumType>(t, this)) {
    // Two sum types match only if they share the same type name (i.e. functor).
    return dynamic_cast<TipCons const *>(t)->arity() == arity() &&
           dynamic_cast<TipSumType const *>(this)->getFunctor() ==
               dynamic_cast<TipSumType const *>(t)->getFunctor();
  }
  return false;
}

TipCons::TipCons(std::vector<std::shared_ptr<TipType>> arguments)
    : arguments(std::move(arguments)) {}

void TipCons::setArguments(std::vector<std::shared_ptr<TipType>> &a) {
  arguments = a;
}

const std::vector<std::shared_ptr<TipType>> &TipCons::getArguments() const {
  return arguments;
}

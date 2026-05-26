#include "TopCons.h"
#include "TopBorrowRef.h"
#include "TopOwningRef.h"
#include "TopSumType.h"
#include "TopTypeVisitor.h"

std::size_t TopCons::arity() const { return arguments.size(); }

namespace {
/*! \brief Determine that subtypes of two objects are the same
 */
template <typename T> bool sameType(const TopType *x, const TopType *y) {
  return dynamic_cast<T const *>(x) && dynamic_cast<T const *>(y);
}
} // namespace

/*! \brief Check for dynamic subtype and artity agreement
 * We explicitly test the types here which is not robust to
 * the addition of new subtypes of TopCons.  Extend this if you
 * add such a subtype.
 */
bool TopCons::doMatch(TopType const *t) const {
  // Check if they are both the same TopType subtype
  if (sameType<TopFunction>(t, this) || sameType<TopInt>(t, this) ||
      sameType<TopRef>(t, this) || sameType<TopOwningRef>(t, this) ||
      sameType<TopBorrowRef>(t, this)) {
    auto topCons = dynamic_cast<TopCons const *>(t);
    return topCons->arity() == arity();
  }
  if (sameType<TopSumType>(t, this)) {
    // Two sum types match only if they share the same type name (i.e. functor).
    return dynamic_cast<TopCons const *>(t)->arity() == arity() &&
           dynamic_cast<TopSumType const *>(this)->getFunctor() ==
               dynamic_cast<TopSumType const *>(t)->getFunctor();
  }
  return false;
}

TopCons::TopCons(std::vector<std::shared_ptr<TopType>> arguments)
    : arguments(std::move(arguments)) {}

void TopCons::setArguments(std::vector<std::shared_ptr<TopType>> &a) {
  arguments = a;
}

const std::vector<std::shared_ptr<TopType>> &TopCons::getArguments() const {
  return arguments;
}

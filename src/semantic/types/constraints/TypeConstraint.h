#pragma once

#include "TopType.h"

/*!
 * \class TypeConstraint
 *
 * \brief A simple type constraint representation.
 */
class TypeConstraint {
public:
  TypeConstraint() = delete;
  TypeConstraint(std::shared_ptr<TopType> l, std::shared_ptr<TopType> r);

  std::shared_ptr<TopType> lhs;
  std::shared_ptr<TopType> rhs;
  bool operator==(const TypeConstraint &other) const;
  bool operator!=(const TypeConstraint &other) const;
  friend std::ostream &operator<<(std::ostream &os, const TypeConstraint &obj);
};

#pragma once

#include <iostream>
#include <memory>
#include <sstream>
#include <vector>

class TipTypeVisitor;

/*!
 * \class TipType
 *
 * \brief Abstract base class for all TIP types.
 */
class TipType {
public:
  virtual ~TipType() = default;

  //! Print the type to an output stream
  virtual std::ostream &print(std::ostream &out) const = 0;

  //! Accept a visitor
  virtual void accept(TipTypeVisitor *visitor) = 0;

  //! Compare two types for equality
  virtual bool operator==(const TipType &other) const = 0;

  bool operator!=(const TipType &other) const { return !(*this == other); }

  // ========== TipType structural interface ==========

  /*! Returns the direct child types (arguments) of this type.
   *  Default is empty; TipCons overrides to return arguments.
   *  \throws InternalError for TipMu (must not enter the solver). */
  virtual std::vector<std::shared_ptr<TipType>> getChildTypes() const { return {}; }

  /*! Reconstruct this type with a new set of child types.
   *  \throws InternalError for TipMu (must not enter the solver). */
  virtual std::shared_ptr<TipType> withChildTypes(
      std::vector<std::shared_ptr<TipType>> children) const = 0;

  // ========== Solver-facing type predicates / functors ==========

  virtual bool isVariable() const { return false; }
  virtual std::string getFunctor() const = 0;
  virtual std::size_t arity() const { return 0; }

  std::string toString() const {
    std::ostringstream oss;
    print(oss);
    return oss.str();
  }
};

//! Stream insertion operator for TipType
inline std::ostream &operator<<(std::ostream &out, const TipType &t) {
  return t.print(out);
}

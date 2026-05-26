#pragma once

#include <iostream>
#include <memory>
#include <sstream>
#include <vector>

class TopTypeVisitor;

/*!
 * \class TopType
 *
 * \brief Abstract base class for all TOP types.
 */
class TopType {
public:
  virtual ~TopType() = default;

  //! Print the type to an output stream
  virtual std::ostream &print(std::ostream &out) const = 0;

  //! Accept a visitor
  virtual void accept(TopTypeVisitor *visitor) = 0;

  //! Compare two types for equality
  virtual bool operator==(const TopType &other) const = 0;

  bool operator!=(const TopType &other) const { return !(*this == other); }

  // ========== TopType structural interface ==========

  /*! Returns the direct child types (arguments) of this type.
   *  Default is empty; TopCons overrides to return arguments.
   *  \throws InternalError for TopMu (must not enter the solver). */
  virtual std::vector<std::shared_ptr<TopType>> getChildTypes() const { return {}; }

  /*! Reconstruct this type with a new set of child types.
   *  \throws InternalError for TopMu (must not enter the solver). */
  virtual std::shared_ptr<TopType> withChildTypes(
      std::vector<std::shared_ptr<TopType>> children) const = 0;

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

//! Stream insertion operator for TopType
inline std::ostream &operator<<(std::ostream &out, const TopType &t) {
  return t.print(out);
}

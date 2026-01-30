#pragma once

#include "TermInterface.h"
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
class TipType : public Term {
public:
  virtual ~TipType() = default;

  //! Print the type to an output stream
  virtual std::ostream &print(std::ostream &out) const = 0;

  //! Accept a visitor
  virtual void accept(TipTypeVisitor *visitor) = 0;

  //! Compare two types for equality
  virtual bool operator==(const TipType &other) const = 0;

  bool operator!=(const TipType &other) const { return !(*this == other); }

  // ========== Term interface implementation ==========

  bool isVariable() const override { return false; }

  std::string toString() const override {
    std::ostringstream oss;
    print(oss);
    return oss.str();
  }

  bool equals(const Term &other) const override {
    if (auto *otherTip = dynamic_cast<const TipType *>(&other)) {
      return *this == *otherTip;
    }
    return false;
  }
};

//! Stream insertion operator for TipType
inline std::ostream &operator<<(std::ostream &out, const TipType &t) {
  return t.print(out);
}

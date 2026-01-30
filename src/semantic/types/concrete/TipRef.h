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

  // ========== Term interface ==========
  std::string getFunctor() const override { return "ptr"; }
  std::size_t arity() const override { return 1; }
  std::vector<std::shared_ptr<Term>> getSubterms() const override;
  std::shared_ptr<Term> withSubterms(std::vector<std::shared_ptr<Term>> newSubterms) const override;

protected:
  std::ostream &print(std::ostream &out) const override;
};

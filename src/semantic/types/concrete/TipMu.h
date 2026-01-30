#pragma once

#include "TipType.h"
#include <memory>
#include <string>
#include <vector>

class TipVar;
class TipTypeVisitor;

/*!
 * \class TipMu
 *
 * \brief A proper type representing a recursive type (μα.T).
 */
class TipMu : public TipType {
public:
  TipMu() = delete;
  TipMu(std::shared_ptr<TipVar> v, std::shared_ptr<TipType> t);

  const std::shared_ptr<TipVar> &getV() const;
  const std::shared_ptr<TipType> &getT() const;

  bool operator==(const TipType &other) const override;
  bool operator!=(const TipType &other) const;

  void accept(TipTypeVisitor *visitor) override;

  // ========== Term interface ==========
  std::string getFunctor() const override { return "μ"; }
  std::size_t arity() const override { return 2; }
  std::vector<std::shared_ptr<Term>> getSubterms() const override;
  std::shared_ptr<Term> withSubterms(std::vector<std::shared_ptr<Term>> newSubterms) const override;

protected:
  std::ostream &print(std::ostream &out) const override;

private:
  std::shared_ptr<TipVar> v;
  std::shared_ptr<TipType> t;
};

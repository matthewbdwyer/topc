#pragma once

#include "TipCons.h"
#include <memory>
#include <string>
#include <vector>

class TipTypeVisitor;

/*!
 * \class TipRecord
 *
 * \brief A proper type representing a record type.
 */
class TipRecord : public TipCons {
public:
  TipRecord() = delete;
  TipRecord(std::vector<std::shared_ptr<TipType>> inits,
            std::vector<std::string> names);

  bool operator==(const TipType &other) const override;
  bool operator!=(const TipType &other) const;

  std::vector<std::string> const &getNames() const;
  std::vector<std::shared_ptr<TipType>> &getInits();

  void accept(TipTypeVisitor *visitor) override;

  // ========== Term interface ==========
  std::string getFunctor() const override;
  std::size_t arity() const override;
  std::vector<std::shared_ptr<Term>> getSubterms() const override;
  std::shared_ptr<Term> withSubterms(std::vector<std::shared_ptr<Term>> newSubterms) const override;

protected:
  std::ostream &print(std::ostream &out) const override;

private:
  std::vector<std::string> names;
};

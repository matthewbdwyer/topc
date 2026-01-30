#pragma once

#include "TipType.h"
#include <memory>
#include <vector>

class TipTypeVisitor;

/*!
 * \class TipCons
 *
 * \brief Abstract base for compound TIP types (constructors).
 */
class TipCons : public TipType {
public:
  TipCons() = default;

  const std::vector<std::shared_ptr<TipType>> &getArguments() const;
  void setArguments(std::vector<std::shared_ptr<TipType>> &args);
  std::size_t arity() const override;
  bool doMatch(TipType const *t) const;

protected:
  TipCons(std::vector<std::shared_ptr<TipType>> arguments);
  std::vector<std::shared_ptr<TipType>> arguments;
};

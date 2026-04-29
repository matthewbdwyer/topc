#pragma once

#include "TipCons.h"
#include "TipType.h"
#include <memory>
#include <string>
#include <vector>

class TipTypeVisitor;

/*!
 * \class TipFunction
 *
 * \brief A proper type representing a function type.
 */
class TipFunction : public TipCons {
public:
  TipFunction() = delete;
  TipFunction(std::vector<std::shared_ptr<TipType>> params,
              std::shared_ptr<TipType> ret);

  std::vector<std::shared_ptr<TipType>> getParamTypes() const;
  std::shared_ptr<TipType> getReturnType() const;

  bool operator==(const TipType &other) const override;
  bool operator!=(const TipType &other) const;

  void accept(TipTypeVisitor *visitor) override;

  // ========== Solver-facing interface ==========
  std::string getFunctor() const override { return "->"; }
  std::size_t arity() const override;

  // ========== TipType structural interface ==========
  std::shared_ptr<TipType> withChildTypes(
      std::vector<std::shared_ptr<TipType>> children) const override;

protected:
  std::ostream &print(std::ostream &out) const override;

private:
  static std::vector<std::shared_ptr<TipType>> combine(
      std::vector<std::shared_ptr<TipType>> params, std::shared_ptr<TipType> ret);
};

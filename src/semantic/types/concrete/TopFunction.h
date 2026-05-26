#pragma once

#include "TopCons.h"
#include "TopType.h"
#include <memory>
#include <string>
#include <vector>

class TopTypeVisitor;

/*!
 * \class TopFunction
 *
 * \brief A proper type representing a function type.
 */
class TopFunction : public TopCons {
public:
  TopFunction() = delete;
  TopFunction(std::vector<std::shared_ptr<TopType>> params,
              std::shared_ptr<TopType> ret);

  std::vector<std::shared_ptr<TopType>> getParamTypes() const;
  std::shared_ptr<TopType> getReturnType() const;

  bool operator==(const TopType &other) const override;
  bool operator!=(const TopType &other) const;

  void accept(TopTypeVisitor *visitor) override;

  // ========== Solver-facing interface ==========
  std::string getFunctor() const override { return "->"; }
  std::size_t arity() const override;

  // ========== TopType structural interface ==========
  std::shared_ptr<TopType> withChildTypes(
      std::vector<std::shared_ptr<TopType>> children) const override;

protected:
  std::ostream &print(std::ostream &out) const override;

private:
  static std::vector<std::shared_ptr<TopType>> combine(
      std::vector<std::shared_ptr<TopType>> params, std::shared_ptr<TopType> ret);
};

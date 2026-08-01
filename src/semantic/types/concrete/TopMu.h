#pragma once

#include "TopType.h"
#include <memory>
#include <string>
#include <vector>

class TopVar;
class TopTypeVisitor;

/*!
 * \class TopMu
 *
 * \brief A proper type representing a recursive type (μα.T).
 */
class TopMu : public TopType {
public:
  TopMu() = delete;
  TopMu(std::shared_ptr<TopVar> v, std::shared_ptr<TopType> t);

  const std::shared_ptr<TopVar> &getV() const;
  const std::shared_ptr<TopType> &getT() const;

  bool operator==(const TopType &other) const override;
  bool operator!=(const TopType &other) const;

  void accept(TopTypeVisitor *visitor) override;

  // ========== Solver-facing interface ==========
  std::string getFunctor() const override { return "μ"; }
  std::size_t arity() const override { return 2; }

  // ========== TopType structural interface ==========
  std::vector<std::shared_ptr<TopType>> getChildTypes() const override;
  std::shared_ptr<TopType> withChildTypes(
      std::vector<std::shared_ptr<TopType>> children) const override;

protected:
  std::ostream &print(std::ostream &out) const override;

private:
  std::shared_ptr<TopVar> v;
  std::shared_ptr<TopType> t;
};

#pragma once

#include "TopCons.h"
#include <string>
#include <vector>
#include <memory>

class TopTypeVisitor;

/*!
 * \class TopInt
 *
 * \brief A proper type representing an int
 */
class TopInt : public TopCons {
public:
  TopInt();

  bool operator==(const TopType &other) const override;
  bool operator!=(const TopType &other) const;

  void accept(TopTypeVisitor *visitor) override;

  // ========== Solver-facing interface ==========
  std::string getFunctor() const override { return "int"; }
  std::size_t arity() const override { return 0; }

  // ========== TopType structural interface ==========
  std::shared_ptr<TopType> withChildTypes(
      std::vector<std::shared_ptr<TopType>> children) const override;

protected:
  std::ostream &print(std::ostream &out) const override;
};

#pragma once

#include "TopCons.h"

#include <memory>
#include <vector>

class TopTypeVisitor;

/*! \brief A proper type representing an absent record field. */
class TopAbsentField : public TopCons {
public:
  TopAbsentField();

  bool operator==(const TopType &other) const override;
  bool operator!=(const TopType &other) const;

  void accept(TopTypeVisitor *visitor) override;

  std::string getFunctor() const override { return "absent"; }
  std::size_t arity() const override { return 0; }

  std::shared_ptr<TopType> withChildTypes(
      std::vector<std::shared_ptr<TopType>> children) const override;

protected:
  std::ostream &print(std::ostream &out) const override;
};
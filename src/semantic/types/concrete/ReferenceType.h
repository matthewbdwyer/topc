#pragma once

#include "TopCons.h"
#include "ReferenceMode.h"
#include <memory>
#include <string>
#include <vector>

class TopTypeVisitor;

class ReferenceType : public TopCons {
public:
  ReferenceType() = delete;
  ReferenceType(std::shared_ptr<TopType> mode,
                std::shared_ptr<TopType> referencedType);

  std::shared_ptr<TopType> getMode() const;
  std::shared_ptr<TopType> getReferencedType() const;

  bool operator==(const TopType &other) const override;
  bool operator!=(const TopType &other) const;

  void accept(TopTypeVisitor *visitor) override;

  std::string getFunctor() const override { return "ref"; }
  std::size_t arity() const override { return 2; }

  std::shared_ptr<TopType> withChildTypes(
      std::vector<std::shared_ptr<TopType>> children) const override;

protected:
  std::ostream &print(std::ostream &out) const override;
};

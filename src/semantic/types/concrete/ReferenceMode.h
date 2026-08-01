#pragma once

#include "TopType.h"
#include <memory>
#include <string>
#include <vector>

class TopTypeVisitor;

class ReferenceMode : public TopType {
public:
  enum class Mode { Own, Borrow };

  explicit ReferenceMode(Mode mode);

  Mode getMode() const;

  bool operator==(const TopType &other) const override;
  bool operator!=(const TopType &other) const;

  void accept(TopTypeVisitor *visitor) override;

  std::string getFunctor() const override;
  std::size_t arity() const override { return 0; }

  std::shared_ptr<TopType> withChildTypes(
      std::vector<std::shared_ptr<TopType>> children) const override;

protected:
  std::ostream &print(std::ostream &out) const override;

private:
  Mode mode;
};

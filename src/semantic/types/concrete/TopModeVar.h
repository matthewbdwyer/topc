#pragma once

#include "TopType.h"
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

class TopTypeVisitor;

class TopModeVar : public TopType {
public:
  TopModeVar();
  explicit TopModeVar(std::size_t id);

  std::size_t getId() const;

  bool operator==(const TopType &other) const override;
  bool operator!=(const TopType &other) const;

  void accept(TopTypeVisitor *visitor) override;

  bool isVariable() const override { return true; }
  std::string getFunctor() const override;
  std::size_t arity() const override { return 0; }

  std::shared_ptr<TopType> withChildTypes(
      std::vector<std::shared_ptr<TopType>> children) const override;

protected:
  std::ostream &print(std::ostream &out) const override;

private:
  std::size_t id;
};

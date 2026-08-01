#pragma once

#include "TopType.h"
#include <memory>
#include <vector>

class TopTypeVisitor;

/*!
 * \class TopCons
 *
 * \brief Abstract base for compound TOP types (constructors).
 */
class TopCons : public TopType {
public:
  TopCons() = default;

  const std::vector<std::shared_ptr<TopType>> &getArguments() const;
  void setArguments(std::vector<std::shared_ptr<TopType>> &args);
  std::size_t arity() const override;
  bool doMatch(TopType const *t) const;

  std::vector<std::shared_ptr<TopType>> getChildTypes() const override {
    return arguments;
  }

protected:
  TopCons(std::vector<std::shared_ptr<TopType>> arguments);
  std::vector<std::shared_ptr<TopType>> arguments;
};

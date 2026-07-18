#pragma once

#include "TopCons.h"

#include <memory>
#include <string>
#include <vector>

class TopTypeVisitor;

/*! \brief A proper type representing a record type. */
class TopRecord : public TopCons {
public:
  TopRecord() = delete;
  TopRecord(std::vector<std::shared_ptr<TopType>> inits,
            std::vector<std::string> names);

  bool operator==(const TopType &other) const override;
  bool operator!=(const TopType &other) const;

  const std::vector<std::string> &getNames() const;
  std::vector<std::shared_ptr<TopType>> &getInits();
  const std::vector<std::shared_ptr<TopType>> &getInits() const;

  void accept(TopTypeVisitor *visitor) override;

  std::string getFunctor() const override;
  std::size_t arity() const override;

  std::shared_ptr<TopType> withChildTypes(
      std::vector<std::shared_ptr<TopType>> children) const override;

protected:
  std::ostream &print(std::ostream &out) const override;

private:
  std::vector<std::string> names;
};
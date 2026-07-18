#include "TopAbsentField.h"
#include "TopTypeVisitor.h"

#include <stdexcept>

TopAbsentField::TopAbsentField() = default;

bool TopAbsentField::operator==(const TopType &other) const {
  return dynamic_cast<const TopAbsentField *>(&other) != nullptr;
}

bool TopAbsentField::operator!=(const TopType &other) const {
  return !(*this == other);
}

std::ostream &TopAbsentField::print(std::ostream &out) const {
  out << std::string("\u25C7");
  return out;
}

void TopAbsentField::accept(TopTypeVisitor *visitor) {
  visitor->visit(this);
  visitor->endVisit(this);
}

std::shared_ptr<TopType> TopAbsentField::withChildTypes(
    std::vector<std::shared_ptr<TopType>> children) const {
  if (!children.empty()) {
    throw std::invalid_argument("TopAbsentField has no child types");
  }
  return std::make_shared<TopAbsentField>();
}
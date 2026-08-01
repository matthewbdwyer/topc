#include "ReferenceMode.h"
#include "TopTypeVisitor.h"

#include <stdexcept>

ReferenceMode::ReferenceMode(Mode mode) : mode(mode) {}

ReferenceMode::Mode ReferenceMode::getMode() const { return mode; }

bool ReferenceMode::operator==(const TopType &other) const {
  auto otherMode = dynamic_cast<const ReferenceMode *>(&other);
  return otherMode != nullptr && mode == otherMode->mode;
}

bool ReferenceMode::operator!=(const TopType &other) const {
  return !(*this == other);
}

void ReferenceMode::accept(TopTypeVisitor *visitor) {
  visitor->visit(this);
  visitor->endVisit(this);
}

std::string ReferenceMode::getFunctor() const {
  return mode == Mode::Own ? "ownmode" : "borrowmode";
}

std::shared_ptr<TopType> ReferenceMode::withChildTypes(
    std::vector<std::shared_ptr<TopType>> children) const {
  if (!children.empty()) {
    throw std::invalid_argument("ReferenceMode has no child types");
  }
  return std::make_shared<ReferenceMode>(mode);
}

std::ostream &ReferenceMode::print(std::ostream &out) const {
  out << (mode == Mode::Own ? "Own" : "Borrow");
  return out;
}

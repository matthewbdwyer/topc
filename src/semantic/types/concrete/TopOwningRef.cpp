#include "TopOwningRef.h"
#include "TopTypeVisitor.h"

#include <sstream>
#include <stdexcept>

TopOwningRef::TopOwningRef(std::shared_ptr<TopType> of)
    : TopCons(std::move(std::vector<std::shared_ptr<TopType>>{of})) {}

bool TopOwningRef::operator==(const TopType &other) const {
  auto otherRef = dynamic_cast<const TopOwningRef *>(&other);
  if (!otherRef) {
    return false;
  }
  return *arguments.front() == *otherRef->getReferencedType();
}

bool TopOwningRef::operator!=(const TopType &other) const {
  return !(*this == other);
}

std::ostream &TopOwningRef::print(std::ostream &out) const {
  out << "own&" << *arguments.front();
  return out;
}

std::shared_ptr<TopType> TopOwningRef::getReferencedType() const {
  return arguments.front();
}

void TopOwningRef::accept(TopTypeVisitor *visitor) {
  if (visitor->visit(this)) {
    for (auto a : arguments) {
      a->accept(visitor);
    }
  }
  visitor->endVisit(this);
}

std::shared_ptr<TopType> TopOwningRef::withChildTypes(
    std::vector<std::shared_ptr<TopType>> children) const {
  if (children.size() != 1) {
    throw std::invalid_argument("TopOwningRef requires exactly 1 child type");
  }
  return std::make_shared<TopOwningRef>(children[0]);
}

#include "TopOwningRef.h"
#include "ReferenceMode.h"
#include "TopTypeVisitor.h"

#include <sstream>
#include <stdexcept>

TopOwningRef::TopOwningRef(std::shared_ptr<TopType> of)
  : ReferenceType(std::make_shared<ReferenceMode>(ReferenceMode::Mode::Own),
       std::move(of)) {}

bool TopOwningRef::operator==(const TopType &other) const {
  auto otherRef = dynamic_cast<const TopOwningRef *>(&other);
  if (!otherRef) {
    return false;
  }
  return *getReferencedType() == *otherRef->getReferencedType();
}

bool TopOwningRef::operator!=(const TopType &other) const {
  return !(*this == other);
}

std::ostream &TopOwningRef::print(std::ostream &out) const {
  out << "own&" << *getReferencedType();
  return out;
}

std::shared_ptr<TopType> TopOwningRef::getReferencedType() const {
  return arguments[1];
}

void TopOwningRef::accept(TopTypeVisitor *visitor) {
  if (visitor->visit(this)) {
    getReferencedType()->accept(visitor);
  }
  visitor->endVisit(this);
}

std::shared_ptr<TopType> TopOwningRef::withChildTypes(
    std::vector<std::shared_ptr<TopType>> children) const {
  if (children.size() != 2) {
    throw std::invalid_argument("TopOwningRef requires exactly 2 child types");
  }
  return ReferenceType::withChildTypes(std::move(children));
}

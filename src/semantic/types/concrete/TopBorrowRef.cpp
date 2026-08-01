#include "TopBorrowRef.h"
#include "ReferenceMode.h"
#include "TopTypeVisitor.h"

#include <sstream>
#include <stdexcept>

TopBorrowRef::TopBorrowRef(std::shared_ptr<TopType> of)
  : ReferenceType(std::make_shared<ReferenceMode>(ReferenceMode::Mode::Borrow),
       std::move(of)) {}

bool TopBorrowRef::operator==(const TopType &other) const {
  auto otherRef = dynamic_cast<const TopBorrowRef *>(&other);
  if (!otherRef) {
    return false;
  }
  return *getReferencedType() == *otherRef->getReferencedType();
}

bool TopBorrowRef::operator!=(const TopType &other) const {
  return !(*this == other);
}

std::ostream &TopBorrowRef::print(std::ostream &out) const {
  out << "borrow&" << *getReferencedType();
  return out;
}

std::shared_ptr<TopType> TopBorrowRef::getReferencedType() const {
  return arguments[1];
}

void TopBorrowRef::accept(TopTypeVisitor *visitor) {
  if (visitor->visit(this)) {
    getReferencedType()->accept(visitor);
  }
  visitor->endVisit(this);
}

std::shared_ptr<TopType> TopBorrowRef::withChildTypes(
    std::vector<std::shared_ptr<TopType>> children) const {
  if (children.size() != 2) {
    throw std::invalid_argument("TopBorrowRef requires exactly 2 child types");
  }
  return ReferenceType::withChildTypes(std::move(children));
}

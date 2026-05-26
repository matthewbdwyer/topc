#include "TopBorrowRef.h"
#include "TopTypeVisitor.h"

#include <sstream>
#include <stdexcept>

TopBorrowRef::TopBorrowRef(std::shared_ptr<TopType> of)
    : TopCons(std::move(std::vector<std::shared_ptr<TopType>>{of})) {}

bool TopBorrowRef::operator==(const TopType &other) const {
  auto otherRef = dynamic_cast<const TopBorrowRef *>(&other);
  if (!otherRef) {
    return false;
  }
  return *arguments.front() == *otherRef->getReferencedType();
}

bool TopBorrowRef::operator!=(const TopType &other) const {
  return !(*this == other);
}

std::ostream &TopBorrowRef::print(std::ostream &out) const {
  out << "borrow&" << *arguments.front();
  return out;
}

std::shared_ptr<TopType> TopBorrowRef::getReferencedType() const {
  return arguments.front();
}

void TopBorrowRef::accept(TopTypeVisitor *visitor) {
  if (visitor->visit(this)) {
    for (auto a : arguments) {
      a->accept(visitor);
    }
  }
  visitor->endVisit(this);
}

std::shared_ptr<TopType> TopBorrowRef::withChildTypes(
    std::vector<std::shared_ptr<TopType>> children) const {
  if (children.size() != 1) {
    throw std::invalid_argument("TopBorrowRef requires exactly 1 child type");
  }
  return std::make_shared<TopBorrowRef>(children[0]);
}

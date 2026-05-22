#include "TipBorrowRef.h"
#include "TipTypeVisitor.h"

#include <sstream>
#include <stdexcept>

TipBorrowRef::TipBorrowRef(std::shared_ptr<TipType> of)
    : TipCons(std::move(std::vector<std::shared_ptr<TipType>>{of})) {}

bool TipBorrowRef::operator==(const TipType &other) const {
  auto otherRef = dynamic_cast<const TipBorrowRef *>(&other);
  if (!otherRef) {
    return false;
  }
  return *arguments.front() == *otherRef->getReferencedType();
}

bool TipBorrowRef::operator!=(const TipType &other) const {
  return !(*this == other);
}

std::ostream &TipBorrowRef::print(std::ostream &out) const {
  out << "borrow&" << *arguments.front();
  return out;
}

std::shared_ptr<TipType> TipBorrowRef::getReferencedType() const {
  return arguments.front();
}

void TipBorrowRef::accept(TipTypeVisitor *visitor) {
  if (visitor->visit(this)) {
    for (auto a : arguments) {
      a->accept(visitor);
    }
  }
  visitor->endVisit(this);
}

std::shared_ptr<TipType> TipBorrowRef::withChildTypes(
    std::vector<std::shared_ptr<TipType>> children) const {
  if (children.size() != 1) {
    throw std::invalid_argument("TipBorrowRef requires exactly 1 child type");
  }
  return std::make_shared<TipBorrowRef>(children[0]);
}

#include "TipOwningRef.h"
#include "TipTypeVisitor.h"

#include <sstream>
#include <stdexcept>

TipOwningRef::TipOwningRef(std::shared_ptr<TipType> of)
    : TipCons(std::move(std::vector<std::shared_ptr<TipType>>{of})) {}

bool TipOwningRef::operator==(const TipType &other) const {
  auto otherRef = dynamic_cast<const TipOwningRef *>(&other);
  if (!otherRef) {
    return false;
  }
  return *arguments.front() == *otherRef->getReferencedType();
}

bool TipOwningRef::operator!=(const TipType &other) const {
  return !(*this == other);
}

std::ostream &TipOwningRef::print(std::ostream &out) const {
  out << "own&" << *arguments.front();
  return out;
}

std::shared_ptr<TipType> TipOwningRef::getReferencedType() const {
  return arguments.front();
}

void TipOwningRef::accept(TipTypeVisitor *visitor) {
  if (visitor->visit(this)) {
    for (auto a : arguments) {
      a->accept(visitor);
    }
  }
  visitor->endVisit(this);
}

std::shared_ptr<TipType> TipOwningRef::withChildTypes(
    std::vector<std::shared_ptr<TipType>> children) const {
  if (children.size() != 1) {
    throw std::invalid_argument("TipOwningRef requires exactly 1 child type");
  }
  return std::make_shared<TipOwningRef>(children[0]);
}

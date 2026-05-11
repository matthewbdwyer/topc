#include "TipRef.h"
#include "TipTypeVisitor.h"

#include <sstream>

TipRef::TipRef(std::shared_ptr<TipType> of)
    : TipCons(std::move(std::vector<std::shared_ptr<TipType>>{of})) {}

bool TipRef::operator==(const TipType &other) const {
  auto otherTipRef = dynamic_cast<const TipRef *>(&other);
  if (!otherTipRef) {
    return false;
  }

  return *arguments.front() == *otherTipRef->getReferencedType();
}

bool TipRef::operator!=(const TipType &other) const {
  return !(*this == other);
}

std::ostream &TipRef::print(std::ostream &out) const {
  out << "\u2B61" << *arguments.front();
  return out;
}

std::shared_ptr<TipType> TipRef::getReferencedType() const {
  return arguments.front();
}

void TipRef::accept(TipTypeVisitor *visitor) {
  if (visitor->visit(this)) {
    for (auto a : arguments) {
      a->accept(visitor);
    }
  }
  visitor->endVisit(this);
}

std::shared_ptr<TipType> TipRef::withChildTypes(
    std::vector<std::shared_ptr<TipType>> children) const {
  if (children.size() != 1) {
    throw std::invalid_argument("TipRef requires exactly 1 child type");
  }
  return std::make_shared<TipRef>(children[0]);
}

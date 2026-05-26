#include "TopRef.h"
#include "TopTypeVisitor.h"

#include <sstream>

TopRef::TopRef(std::shared_ptr<TopType> of)
    : TopCons(std::move(std::vector<std::shared_ptr<TopType>>{of})) {}

bool TopRef::operator==(const TopType &other) const {
  auto otherTopRef = dynamic_cast<const TopRef *>(&other);
  if (!otherTopRef) {
    return false;
  }

  return *arguments.front() == *otherTopRef->getReferencedType();
}

bool TopRef::operator!=(const TopType &other) const {
  return !(*this == other);
}

std::ostream &TopRef::print(std::ostream &out) const {
  out << "\u2B61" << *arguments.front();
  return out;
}

std::shared_ptr<TopType> TopRef::getReferencedType() const {
  return arguments.front();
}

void TopRef::accept(TopTypeVisitor *visitor) {
  if (visitor->visit(this)) {
    for (auto a : arguments) {
      a->accept(visitor);
    }
  }
  visitor->endVisit(this);
}

std::shared_ptr<TopType> TopRef::withChildTypes(
    std::vector<std::shared_ptr<TopType>> children) const {
  if (children.size() != 1) {
    throw std::invalid_argument("TopRef requires exactly 1 child type");
  }
  return std::make_shared<TopRef>(children[0]);
}

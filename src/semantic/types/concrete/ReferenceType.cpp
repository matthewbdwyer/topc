#include "ReferenceType.h"
#include "TopBorrowRef.h"
#include "TopModeVar.h"
#include "TopOwningRef.h"
#include "TopTypeVisitor.h"

#include <stdexcept>

ReferenceType::ReferenceType(std::shared_ptr<TopType> mode,
                             std::shared_ptr<TopType> referencedType)
    : TopCons(std::move(std::vector<std::shared_ptr<TopType>>{
          mode, referencedType})) {}

std::shared_ptr<TopType> ReferenceType::getMode() const { return arguments[0]; }

std::shared_ptr<TopType> ReferenceType::getReferencedType() const {
  return arguments[1];
}

bool ReferenceType::operator==(const TopType &other) const {
  auto otherRef = dynamic_cast<const ReferenceType *>(&other);
  if (!otherRef) {
    return false;
  }
  return *getMode() == *otherRef->getMode() &&
         *getReferencedType() == *otherRef->getReferencedType();
}

bool ReferenceType::operator!=(const TopType &other) const {
  return !(*this == other);
}

void ReferenceType::accept(TopTypeVisitor *visitor) {
  if (visitor->visit(this)) {
    for (auto a : arguments) {
      a->accept(visitor);
    }
  }
  visitor->endVisit(this);
}

std::shared_ptr<TopType> ReferenceType::withChildTypes(
    std::vector<std::shared_ptr<TopType>> children) const {
  if (children.size() != 2) {
    throw std::invalid_argument("ReferenceType requires exactly 2 child types");
  }

  auto mode = std::dynamic_pointer_cast<ReferenceMode>(children[0]);
  if (mode != nullptr) {
    if (mode->getMode() == ReferenceMode::Mode::Own) {
      return std::make_shared<TopOwningRef>(children[1]);
    }
    return std::make_shared<TopBorrowRef>(children[1]);
  }

  if (std::dynamic_pointer_cast<TopModeVar>(children[0]) == nullptr) {
    throw std::invalid_argument("ReferenceType mode child must be a reference mode");
  }
  return std::make_shared<ReferenceType>(children[0], children[1]);
}

std::ostream &ReferenceType::print(std::ostream &out) const {
  auto mode = std::dynamic_pointer_cast<ReferenceMode>(getMode());
  if (mode == nullptr) {
    out << "ref&" << *getReferencedType();
  } else if (mode->getMode() == ReferenceMode::Mode::Own) {
    out << "own&" << *getReferencedType();
  } else {
    out << "borrow&" << *getReferencedType();
  }
  return out;
}

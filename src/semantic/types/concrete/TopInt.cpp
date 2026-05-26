#include "TopInt.h"
#include "TopTypeVisitor.h"

#include <string>

TopInt::TopInt() {}

bool TopInt::operator==(const TopType &other) const {
  auto otherTopInt = dynamic_cast<TopInt const *>(&other);
  if (!otherTopInt) {
    return false;
  }

  return true;
}

bool TopInt::operator!=(const TopType &other) const {
  return !(*this == other);
}

std::ostream &TopInt::print(std::ostream &out) const {
  out << std::string("int");
  return out;
} // LCOV_EXCL_LINE

// TopInt is a 0-ary type constructor so it has no arguments to visit
void TopInt::accept(TopTypeVisitor *visitor) {
  visitor->visit(this);
  visitor->endVisit(this);
}

std::shared_ptr<TopType> TopInt::withChildTypes(
    std::vector<std::shared_ptr<TopType>> children) const {
  if (!children.empty()) {
    throw std::invalid_argument("TopInt has no child types");
  }
  return std::make_shared<TopInt>();
}

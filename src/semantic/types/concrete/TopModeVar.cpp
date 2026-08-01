#include "TopModeVar.h"
#include "TopTypeVisitor.h"

#include <atomic>
#include <sstream>
#include <stdexcept>

namespace {
std::atomic<std::size_t> nextModeVarId{0};
}

TopModeVar::TopModeVar() : id(nextModeVarId++) {}

TopModeVar::TopModeVar(std::size_t id) : id(id) {}

std::size_t TopModeVar::getId() const { return id; }

bool TopModeVar::operator==(const TopType &other) const {
  auto otherVar = dynamic_cast<const TopModeVar *>(&other);
  return otherVar != nullptr && id == otherVar->id;
}

bool TopModeVar::operator!=(const TopType &other) const {
  return !(*this == other);
}

void TopModeVar::accept(TopTypeVisitor *visitor) {
  visitor->visit(this);
  visitor->endVisit(this);
}

std::string TopModeVar::getFunctor() const {
  std::ostringstream oss;
  oss << "modevar@" << id;
  return oss.str();
}

std::shared_ptr<TopType> TopModeVar::withChildTypes(
    std::vector<std::shared_ptr<TopType>> children) const {
  if (!children.empty()) {
    throw std::invalid_argument("TopModeVar has no child types");
  }
  return std::make_shared<TopModeVar>(id);
}

std::ostream &TopModeVar::print(std::ostream &out) const {
  out << "m" << id;
  return out;
}

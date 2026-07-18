#include "TopRecord.h"
#include "TopTypeVisitor.h"

#include <algorithm>
#include <sstream>
#include <stdexcept>

TopRecord::TopRecord(std::vector<std::shared_ptr<TopType>> inits,
                     std::vector<std::string> names)
    : TopCons(std::move(inits)), names(std::move(names)) {}

std::ostream &TopRecord::print(std::ostream &out) const {
  out << "{";
  for (std::size_t i = 0; i < arguments.size(); i++) {
    if (i > 0)
      out << ",";
    out << names.at(i) << ":" << *arguments.at(i);
  }
  out << "}";
  return out;
}

bool TopRecord::operator==(const TopType &other) const {
  auto topRecord = dynamic_cast<const TopRecord *>(&other);
  if (!topRecord)
    return false;

  if (arity() != topRecord->arity())
    return false;

  for (std::size_t i = 0; i < arguments.size(); i++) {
    if (*(arguments.at(i)) != *(topRecord->arguments.at(i)))
      return false;
  }

  return true;
}

bool TopRecord::operator!=(const TopType &other) const {
  return !(*this == other);
}

std::vector<std::shared_ptr<TopType>> &TopRecord::getInits() {
  return arguments;
}

const std::vector<std::shared_ptr<TopType>> &TopRecord::getInits() const {
  return arguments;
}

const std::vector<std::string> &TopRecord::getNames() const { return names; }

void TopRecord::accept(TopTypeVisitor *visitor) {
  if (visitor->visit(this)) {
    for (auto a : arguments) {
      a->accept(visitor);
    }
  }
  visitor->endVisit(this);
}

std::string TopRecord::getFunctor() const {
  std::ostringstream os;
  os << "record{";
  std::vector<std::string> sortedNames = names;
  std::sort(sortedNames.begin(), sortedNames.end());
  for (std::size_t i = 0; i < sortedNames.size(); i++) {
    if (i > 0)
      os << ",";
    os << sortedNames[i];
  }
  os << "}";
  return os.str();
}

std::size_t TopRecord::arity() const { return arguments.size(); }

std::shared_ptr<TopType> TopRecord::withChildTypes(
    std::vector<std::shared_ptr<TopType>> children) const {
  if (children.size() != arguments.size()) {
    throw std::invalid_argument("TopRecord: wrong number of child types");
  }
  return std::make_shared<TopRecord>(std::move(children), names);
}
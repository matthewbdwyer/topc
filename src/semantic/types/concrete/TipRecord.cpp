#include "TipRecord.h"
#include "TipTypeVisitor.h"
#include <algorithm>
#include <sstream>

TipRecord::TipRecord(std::vector<std::shared_ptr<TipType>> inits,
                     std::vector<std::string> names)
    : TipCons(inits), names(names) {}

std::ostream &TipRecord::print(std::ostream &out) const {
  out << "{";
  bool first = true;
  int i = 0;
  for (auto &init : arguments) {
    if (first) {
      out << names.at(i++) << ":" << *init;
      first = false;
      continue;
    }
    out << "," << names.at(i++) << ":" << *init;
  }
  out << "}";
  return out;
}

// This does not obey the semantics of alpha init values
bool TipRecord::operator==(const TipType &other) const {
  auto tipRecord = dynamic_cast<const TipRecord *>(&other);
  if (!tipRecord) {
    return false;
  }

  if (arity() != tipRecord->arity()) {
    return false;
  }

  for (int i = 0; i < arity(); i++) {
    if (*(arguments.at(i)) != *(tipRecord->arguments.at(i))) {
      return false;
    }
  }

  return true;
}

bool TipRecord::operator!=(const TipType &other) const {
  return !(*this == other);
}

std::vector<std::shared_ptr<TipType>> &TipRecord::getInits() {
  return arguments;
}

std::vector<std::string> const &TipRecord::getNames() const { return names; }

void TipRecord::accept(TipTypeVisitor *visitor) {
  if (visitor->visit(this)) {
    for (auto a : arguments) {
      a->accept(visitor);
    }
  }
  visitor->endVisit(this);
}

std::string TipRecord::getFunctor() const {
  std::ostringstream os;
  os << "record{";
  // Create sorted copy of names for consistent functor
  std::vector<std::string> sortedNames = names;
  std::sort(sortedNames.begin(), sortedNames.end());
  bool first = true;
  for (const auto &name : sortedNames) {
    if (!first) os << ",";
    os << name;
    first = false;
  }
  os << "}";
  return os.str();
}

std::size_t TipRecord::arity() const {
  return arguments.size();
}

std::vector<std::shared_ptr<Term>> TipRecord::getSubterms() const {
  std::vector<std::shared_ptr<Term>> result;
  for (const auto &arg : arguments) {
    result.push_back(arg);
  }
  return result;
}

std::shared_ptr<Term> TipRecord::withSubterms(std::vector<std::shared_ptr<Term>> newSubterms) const {
  if (newSubterms.size() != arguments.size()) {
    throw std::invalid_argument("TipRecord: wrong number of subterms");
  }
  std::vector<std::shared_ptr<TipType>> newInits;
  for (const auto &sub : newSubterms) {
    auto t = std::dynamic_pointer_cast<TipType>(sub);
    if (!t) {
      throw std::invalid_argument("TipRecord subterm must be a TipType");
    }
    newInits.push_back(t);
  }
  return std::make_shared<TipRecord>(newInits, names);
}

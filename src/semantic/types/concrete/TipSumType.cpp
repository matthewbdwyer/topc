#include "TipSumType.h"
#include "TipTypeVisitor.h"

#include <stdexcept>

TipSumType::TipSumType(std::string name,
                       std::vector<std::string> ctors,
                       std::vector<std::shared_ptr<TipType>> payloads,
                       std::map<std::string, int> arities)
    : TipCons(std::move(payloads)),
      typeName(std::move(name)),
      ctorOrder(std::move(ctors)),
      ctorArities(std::move(arities)) {}

std::vector<std::shared_ptr<TipType>>
TipSumType::getCtorPayloads(const std::string &tag) const {
  int offset = 0;
  for (auto &c : ctorOrder) {
    int n = ctorArities.at(c);
    if (c == tag) {
      return std::vector<std::shared_ptr<TipType>>(
          arguments.begin() + offset, arguments.begin() + offset + n);
    }
    offset += n;
  }
  return {};
}

bool TipSumType::operator==(const TipType &other) const {
  auto o = dynamic_cast<const TipSumType *>(&other);
  if (!o)
    return false;
  if (typeName != o->typeName)
    return false;
  if (arguments.size() != o->arguments.size())
    return false;
  for (std::size_t i = 0; i < arguments.size(); i++) {
    if (*arguments[i] != *o->arguments[i])
      return false;
  }
  return true;
}

bool TipSumType::operator!=(const TipType &other) const {
  return !(*this == other);
}

void TipSumType::accept(TipTypeVisitor *visitor) {
  if (visitor->visit(this)) {
    for (auto a : arguments) {
      a->accept(visitor);
    }
  }
  visitor->endVisit(this);
}

std::shared_ptr<TipType> TipSumType::withChildTypes(
    std::vector<std::shared_ptr<TipType>> children) const {
  if (children.size() != arguments.size()) {
    throw std::invalid_argument(
        "TipSumType::withChildTypes: wrong number of children");
  }
  return std::make_shared<TipSumType>(typeName, ctorOrder, children,
                                      ctorArities);
}

std::ostream &TipSumType::print(std::ostream &out) const {
  out << typeName << "{";
  int offset = 0;
  for (std::size_t i = 0; i < ctorOrder.size(); i++) {
    if (i > 0)
      out << "|";
    out << ctorOrder[i];
    int n = ctorArities.at(ctorOrder[i]);
    if (n > 0) {
      out << "(";
      for (int j = 0; j < n; j++) {
        if (j > 0)
          out << ",";
        out << *arguments[offset + j];
      }
      out << ")";
    }
    offset += n;
  }
  out << "}";
  return out;
}

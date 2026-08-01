#include "TopSumType.h"
#include "TopTypeVisitor.h"

#include <stdexcept>

TopSumType::TopSumType(std::string name,
                       std::vector<std::string> ctors,
                       std::vector<std::shared_ptr<TopType>> payloads,
                       std::map<std::string, int> arities)
    : TopCons(std::move(payloads)),
      typeName(std::move(name)),
      ctorOrder(std::move(ctors)),
      ctorArities(std::move(arities)) {}

std::vector<std::shared_ptr<TopType>>
TopSumType::getCtorPayloads(const std::string &tag) const {
  int offset = 0;
  for (auto &c : ctorOrder) {
    int n = ctorArities.at(c);
    if (c == tag) {
      return std::vector<std::shared_ptr<TopType>>(
          arguments.begin() + offset, arguments.begin() + offset + n);
    }
    offset += n;
  }
  return {};
}

bool TopSumType::operator==(const TopType &other) const {
  auto o = dynamic_cast<const TopSumType *>(&other);
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

bool TopSumType::operator!=(const TopType &other) const {
  return !(*this == other);
}

void TopSumType::accept(TopTypeVisitor *visitor) {
  if (visitor->visit(this)) {
    for (auto a : arguments) {
      a->accept(visitor);
    }
  }
  visitor->endVisit(this);
}

std::shared_ptr<TopType> TopSumType::withChildTypes(
    std::vector<std::shared_ptr<TopType>> children) const {
  if (children.size() != arguments.size()) {
    throw std::invalid_argument(
        "TopSumType::withChildTypes: wrong number of children");
  }
  return std::make_shared<TopSumType>(typeName, ctorOrder, children,
                                      ctorArities);
}

std::ostream &TopSumType::print(std::ostream &out) const {
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

#include "TipFunction.h"
#include "TipTypeVisitor.h"

#include <sstream>

TipFunction::TipFunction(std::vector<std::shared_ptr<TipType>> params,
                         std::shared_ptr<TipType> ret)
    : TipCons(combine(params, ret)) {}

std::vector<std::shared_ptr<TipType>>
TipFunction::combine(std::vector<std::shared_ptr<TipType>> params,
                     std::shared_ptr<TipType> ret) {
  params.push_back(std::move(ret));
  return params;
}

std::vector<std::shared_ptr<TipType>> TipFunction::getParamTypes() const {
  std::vector<std::shared_ptr<TipType>> params(arguments.begin(),
                                               arguments.end() - 1);
  return params;
}

std::shared_ptr<TipType> TipFunction::getReturnType() const {
  return arguments.back();
}

std::size_t TipFunction::arity() const {
  return arguments.size();  // params + return type stored together
}

std::vector<std::shared_ptr<Term>> TipFunction::getSubterms() const {
  std::vector<std::shared_ptr<Term>> result;
  for (const auto &arg : arguments) {
    result.push_back(arg);
  }
  return result;
}

std::shared_ptr<Term> TipFunction::withSubterms(std::vector<std::shared_ptr<Term>> newSubterms) const {
  if (newSubterms.size() != arguments.size()) {
    throw std::invalid_argument("TipFunction: wrong number of subterms");
  }
  std::vector<std::shared_ptr<TipType>> newParams;
  for (size_t i = 0; i < newSubterms.size() - 1; i++) {
    auto p = std::dynamic_pointer_cast<TipType>(newSubterms[i]);
    if (!p) {
      throw std::invalid_argument("TipFunction subterm must be a TipType");
    }
    newParams.push_back(p);
  }
  auto newRet = std::dynamic_pointer_cast<TipType>(newSubterms.back());
  if (!newRet) {
    throw std::invalid_argument("TipFunction return type must be a TipType");
  }
  return std::make_shared<TipFunction>(newParams, newRet);
}

std::ostream &TipFunction::print(std::ostream &out) const {
  out << "(";
  int end_of_args = arguments.size() - 1;
  for (int i = 0; i < end_of_args; i++) {
    out << *arguments.at(i) << (i == end_of_args - 1 ? "" : ",");
  }
  out << ") -> " << *arguments.back();
  return out;
}

bool TipFunction::operator==(const TipType &other) const {
  auto otherTipFunction = dynamic_cast<const TipFunction *>(&other);
  if (!otherTipFunction) {
    return false;
  }

  if (arguments.size() != otherTipFunction->arguments.size()) {
    return false;
  }

  for (int i = 0; i < arguments.size(); i++) {
    if (*(arguments.at(i)) != *(otherTipFunction->arguments.at(i))) {
      return false;
    }
  }

  return *arguments.back() == *(otherTipFunction->arguments.back());
}

bool TipFunction::operator!=(const TipType &other) const {
  return !(*this == other);
}

void TipFunction::accept(TipTypeVisitor *visitor) {
  if (visitor->visit(this)) {
    for (auto a : arguments) {
      a->accept(visitor);
    }
  }
  visitor->endVisit(this);
}

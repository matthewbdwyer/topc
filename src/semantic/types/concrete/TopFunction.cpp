#include "TopFunction.h"
#include "TopTypeVisitor.h"

#include <sstream>

TopFunction::TopFunction(std::vector<std::shared_ptr<TopType>> params,
                         std::shared_ptr<TopType> ret)
    : TopCons(combine(params, ret)) {}

std::vector<std::shared_ptr<TopType>>
TopFunction::combine(std::vector<std::shared_ptr<TopType>> params,
                     std::shared_ptr<TopType> ret) {
  params.push_back(std::move(ret));
  return params;
}

std::vector<std::shared_ptr<TopType>> TopFunction::getParamTypes() const {
  std::vector<std::shared_ptr<TopType>> params(arguments.begin(),
                                               arguments.end() - 1);
  return params;
}

std::shared_ptr<TopType> TopFunction::getReturnType() const {
  return arguments.back();
}

std::size_t TopFunction::arity() const {
  return arguments.size();  // params + return type stored together
}

std::shared_ptr<TopType> TopFunction::withChildTypes(
    std::vector<std::shared_ptr<TopType>> children) const {
  if (children.size() != arguments.size()) {
    throw std::invalid_argument("TopFunction: wrong number of child types");
  }
  std::vector<std::shared_ptr<TopType>> params(children.begin(),
                                               children.end() - 1);
  return std::make_shared<TopFunction>(params, children.back());
}

std::ostream &TopFunction::print(std::ostream &out) const {
  out << "(";
  int end_of_args = arguments.size() - 1;
  for (int i = 0; i < end_of_args; i++) {
    out << *arguments.at(i) << (i == end_of_args - 1 ? "" : ",");
  }
  out << ") -> " << *arguments.back();
  return out;
}

bool TopFunction::operator==(const TopType &other) const {
  auto otherTopFunction = dynamic_cast<const TopFunction *>(&other);
  if (!otherTopFunction) {
    return false;
  }

  if (arguments.size() != otherTopFunction->arguments.size()) {
    return false;
  }

  for (int i = 0; i < arguments.size(); i++) {
    if (*(arguments.at(i)) != *(otherTopFunction->arguments.at(i))) {
      return false;
    }
  }

  return *arguments.back() == *(otherTopFunction->arguments.back());
}

bool TopFunction::operator!=(const TopType &other) const {
  return !(*this == other);
}

void TopFunction::accept(TopTypeVisitor *visitor) {
  if (visitor->visit(this)) {
    for (auto a : arguments) {
      a->accept(visitor);
    }
  }
  visitor->endVisit(this);
}

#include "TopMu.h"
#include "InternalError.h"
#include "TopTypeVisitor.h"

#include <iostream>

TopMu::TopMu(std::shared_ptr<TopVar> v, std::shared_ptr<TopType> t)
    : v(std::move(v)), t(std::move(t)) {}

const std::shared_ptr<TopVar> &TopMu::getV() const { return v; }

const std::shared_ptr<TopType> &TopMu::getT() const { return t; }

bool TopMu::operator==(const TopType &other) const {
  auto mu = dynamic_cast<const TopMu *>(&other);
  if (!mu) {
    return false;
  }
  return *v == *(mu->v) && *t == *(mu->t);
}

bool TopMu::operator!=(const TopType &other) const { return !(*this == other); }

std::ostream &TopMu::print(std::ostream &out) const {
  out << "\u03bc" << *v << "." << *t;
  return out;
}

void TopMu::accept(TopTypeVisitor *visitor) {
  if (visitor->visit(this)) {
    v->accept(visitor);
    t->accept(visitor);
  }
  visitor->endVisit(this);
}

std::vector<std::shared_ptr<TopType>> TopMu::getChildTypes() const {
  throw InternalError("TopMu::getChildTypes: TopMu is a closure output, not a solver term");
}

std::shared_ptr<TopType> TopMu::withChildTypes(
    std::vector<std::shared_ptr<TopType>>) const {
  throw InternalError("TopMu::withChildTypes: TopMu is a closure output, not a solver term");
}

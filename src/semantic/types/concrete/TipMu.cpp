#include "TipMu.h"
#include "TipTypeVisitor.h"

#include <iostream>

TipMu::TipMu(std::shared_ptr<TipVar> v, std::shared_ptr<TipType> t)
    : v(std::move(v)), t(std::move(t)) {}

const std::shared_ptr<TipVar> &TipMu::getV() const { return v; }

const std::shared_ptr<TipType> &TipMu::getT() const { return t; }

bool TipMu::operator==(const TipType &other) const {
  auto mu = dynamic_cast<const TipMu *>(&other);
  if (!mu) {
    return false;
  }
  return *v == *(mu->v) && *t == *(mu->t);
}

bool TipMu::operator!=(const TipType &other) const { return !(*this == other); }

std::ostream &TipMu::print(std::ostream &out) const {
  out << "\u03bc" << *v << "." << *t;
  return out;
}

void TipMu::accept(TipTypeVisitor *visitor) {
  if (visitor->visit(this)) {
    v->accept(visitor);
    t->accept(visitor);
  }
  visitor->endVisit(this);
}

std::vector<std::shared_ptr<Term>> TipMu::getSubterms() const {
  return {v, t};
}

std::shared_ptr<Term> TipMu::withSubterms(std::vector<std::shared_ptr<Term>> newSubterms) const {
  if (newSubterms.size() != 2) {
    throw std::invalid_argument("TipMu requires exactly 2 subterms");
  }
  auto newV = std::dynamic_pointer_cast<TipVar>(newSubterms[0]);
  if (!newV) {
    throw std::invalid_argument("TipMu first subterm must be a TipVar");
  }
  auto newT = std::dynamic_pointer_cast<TipType>(newSubterms[1]);
  if (!newT) {
    throw std::invalid_argument("TipMu second subterm must be a TipType");
  }
  return std::make_shared<TipMu>(newV, newT);
}

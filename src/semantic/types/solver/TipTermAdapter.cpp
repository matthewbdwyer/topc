#include "TipTermAdapter.h"

#include "InternalError.h"
#include "TipAlpha.h"
#include "TipMu.h"
#include "TipVar.h"

// ---------------------------------------------------------------------------
// Construction / wrap / unwrap
// ---------------------------------------------------------------------------

TipTermAdapter::TipTermAdapter(std::shared_ptr<TipType> t)
    : tipType(std::move(t)) {}

std::shared_ptr<TipTermAdapter>
TipTermAdapter::wrap(std::shared_ptr<TipType> t) {
  if (std::dynamic_pointer_cast<TipMu>(t)) {
    throw InternalError(
        "TipTermAdapter::wrap: TipMu is a closure-phase output, "
        "not a valid solver input");
  }
  return std::shared_ptr<TipTermAdapter>(new TipTermAdapter(std::move(t)));
}

std::shared_ptr<TipType>
TipTermAdapter::unwrap(std::shared_ptr<Term> t) {
  auto adapter = std::dynamic_pointer_cast<TipTermAdapter>(t);
  if (!adapter) {
    throw InternalError(
        "TipTermAdapter::unwrap: term is not a TipTermAdapter");
  }
  return adapter->getTipType();
}

// ---------------------------------------------------------------------------
// Term interface
// ---------------------------------------------------------------------------

bool TipTermAdapter::isVariable() const {
  // dynamic_pointer_cast<TipVar> catches TipAlpha too (TipAlpha IS-A TipVar).
  return std::dynamic_pointer_cast<TipVar>(tipType) != nullptr;
}

std::string TipTermAdapter::getFunctor() const {
  return tipType->getFunctor();
}

std::size_t TipTermAdapter::arity() const {
  return tipType->arity();
}

std::vector<std::shared_ptr<Term>> TipTermAdapter::getSubterms() const {
  std::vector<std::shared_ptr<Term>> result;
  for (const auto &child : tipType->getChildTypes()) {
    result.push_back(wrap(child));
  }
  return result;
}

std::shared_ptr<Term> TipTermAdapter::withSubterms(
    std::vector<std::shared_ptr<Term>> newSubterms) const {
  // Collect the underlying TipType children from each sub-adapter.
  std::vector<std::shared_ptr<TipType>> tipChildren;
  tipChildren.reserve(newSubterms.size());
  for (const auto &sub : newSubterms) {
    tipChildren.push_back(unwrap(sub));
  }
  return wrap(tipType->withChildTypes(std::move(tipChildren)));
}

std::string TipTermAdapter::toString() const {
  return tipType->toString();
}

bool TipTermAdapter::equals(const Term &other) const {
  if (const auto *otherAdapter =
          dynamic_cast<const TipTermAdapter *>(&other)) {
    return *tipType == *otherAdapter->tipType;
  }
  return false;
}

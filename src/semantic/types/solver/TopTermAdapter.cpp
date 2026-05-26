#include "TopTermAdapter.h"

#include "InternalError.h"
#include "TopAlpha.h"
#include "TopMu.h"
#include "TopVar.h"

// ---------------------------------------------------------------------------
// Construction / wrap / unwrap
// ---------------------------------------------------------------------------

TopTermAdapter::TopTermAdapter(std::shared_ptr<TopType> t)
    : topType(std::move(t)) {}

std::shared_ptr<TopTermAdapter>
TopTermAdapter::wrap(std::shared_ptr<TopType> t) {
  if (std::dynamic_pointer_cast<TopMu>(t)) {
    throw InternalError(
        "TopTermAdapter::wrap: TopMu is a closure-phase output, "
        "not a valid solver input");
  }
  return std::shared_ptr<TopTermAdapter>(new TopTermAdapter(std::move(t)));
}

std::shared_ptr<TopType>
TopTermAdapter::unwrap(std::shared_ptr<Term> t) {
  auto adapter = std::dynamic_pointer_cast<TopTermAdapter>(t);
  if (!adapter) {
    throw InternalError(
        "TopTermAdapter::unwrap: term is not a TopTermAdapter");
  }
  return adapter->getTopType();
}

// ---------------------------------------------------------------------------
// Term interface
// ---------------------------------------------------------------------------

bool TopTermAdapter::isVariable() const {
  // dynamic_pointer_cast<TopVar> catches TopAlpha too (TopAlpha IS-A TopVar).
  return std::dynamic_pointer_cast<TopVar>(topType) != nullptr;
}

std::string TopTermAdapter::getFunctor() const {
  return topType->getFunctor();
}

std::size_t TopTermAdapter::arity() const {
  return topType->arity();
}

std::vector<std::shared_ptr<Term>> TopTermAdapter::getSubterms() const {
  std::vector<std::shared_ptr<Term>> result;
  for (const auto &child : topType->getChildTypes()) {
    result.push_back(wrap(child));
  }
  return result;
}

std::shared_ptr<Term> TopTermAdapter::withSubterms(
    std::vector<std::shared_ptr<Term>> newSubterms) const {
  // Collect the underlying TopType children from each sub-adapter.
  std::vector<std::shared_ptr<TopType>> topChildren;
  topChildren.reserve(newSubterms.size());
  for (const auto &sub : newSubterms) {
    topChildren.push_back(unwrap(sub));
  }
  return wrap(topType->withChildTypes(std::move(topChildren)));
}

std::string TopTermAdapter::toString() const {
  return topType->toString();
}

bool TopTermAdapter::equals(const Term &other) const {
  if (const auto *otherAdapter =
          dynamic_cast<const TopTermAdapter *>(&other)) {
    return *topType == *otherAdapter->topType;
  }
  return false;
}

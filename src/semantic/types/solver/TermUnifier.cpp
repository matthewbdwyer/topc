#include "TermUnifier.h"
#include <set>

void TermUnifier::addConstraint(std::shared_ptr<Term> t1, std::shared_ptr<Term> t2) {
  constraints.emplace_back(std::move(t1), std::move(t2));
}

void TermUnifier::solve() {
  while (!constraints.empty()) {
    auto [t1, t2] = constraints.back();
    constraints.pop_back();
    unify(t1, t2);
  }
}

void TermUnifier::unify(std::shared_ptr<Term> t1, std::shared_ptr<Term> t2) {
  t1 = find(t1);
  t2 = find(t2);

  if (t1 == t2 || t1->equals(*t2)) {
    return;
  }

  if (t1->isVariable()) {
    const std::string varName = t1->getFunctor();
    if (occursIn(varName, t2.get())) {
      throw TermUnificationError("Occurs check failed: " + varName + " occurs in " +
                                 t2->toString(), t1, t2);
    }
    substitution[varName] = t2;
    return;
  }

  if (t2->isVariable()) {
    const std::string varName = t2->getFunctor();
    if (occursIn(varName, t1.get())) {
      throw TermUnificationError("Occurs check failed: " + varName + " occurs in " +
                                 t1->toString(), t2, t1);
    }
    substitution[varName] = t1;
    return;
  }

  if (!t1->matchesFunctor(*t2)) {
    throw TermUnificationError("Cannot unify " + t1->toString() + " with " +
                               t2->toString() + ": different structure", t1, t2);
  }

  auto subs1 = t1->getSubterms();
  auto subs2 = t2->getSubterms();
  for (std::size_t i = 0; i < subs1.size(); ++i) {
    constraints.emplace_back(subs1[i], subs2[i]);
  }
}

bool TermUnifier::occursIn(const std::string &varName, const Term *term) const {
  if (term->isVariable()) {
    const std::string termName = term->getFunctor();
    if (termName == varName) {
      return true;
    }
    auto it = substitution.find(termName);
    if (it != substitution.end()) {
      return occursIn(varName, it->second.get());
    }
    return false;
  }

  for (const auto &sub : term->getSubterms()) {
    if (occursIn(varName, sub.get())) {
      return true;
    }
  }
  return false;
}

std::shared_ptr<Term> TermUnifier::find(std::shared_ptr<Term> term) const {
  if (!term->isVariable()) {
    return term;
  }
  auto it = substitution.find(term->getFunctor());
  if (it == substitution.end()) {
    return term;
  }
  return find(it->second);
}

std::shared_ptr<Term> TermUnifier::apply(std::shared_ptr<Term> term) const {
  if (term->isVariable()) {
    auto it = substitution.find(term->getFunctor());
    if (it != substitution.end()) {
      return apply(it->second);
    }
    return term;
  }

  auto subterms = term->getSubterms();
  if (subterms.empty()) {
    return term;
  }

  std::vector<std::shared_ptr<Term>> newSubterms;
  newSubterms.reserve(subterms.size());
  bool changed = false;
  for (const auto &sub : subterms) {
    auto newSub = apply(sub);
    if (newSub != sub) {
      changed = true;
    }
    newSubterms.push_back(newSub);
  }

  if (!changed) {
    return term;
  }
  return term->withSubterms(std::move(newSubterms));
}

bool TermUnifier::isBound(const std::string &varName) const {
  return substitution.find(varName) != substitution.end();
}

std::shared_ptr<Term> TermUnifier::lookup(const std::string &varName) const {
  auto it = substitution.find(varName);
  if (it == substitution.end()) {
    return nullptr;
  }
  return it->second;
}

void TermUnifier::close(CycleHandler cycleHandler) {
  std::set<std::string> resolving;

  std::function<std::shared_ptr<Term>(std::shared_ptr<Term>)> closeTerm =
      [&](std::shared_ptr<Term> term) -> std::shared_ptr<Term> {
    if (term->isVariable()) {
      const std::string varName = term->getFunctor();

      if (resolving.count(varName)) {
        if (cycleHandler) {
          return cycleHandler(varName, term);
        }
        throw TermUnificationError("Cycle detected for variable " + varName, term, term);
      }

      auto it = substitution.find(varName);
      if (it == substitution.end()) {
        return term;
      }

      resolving.insert(varName);
      auto closed = closeTerm(it->second);
      resolving.erase(varName);

      substitution[varName] = closed;
      return closed;
    }

    auto subterms = term->getSubterms();
    if (subterms.empty()) {
      return term;
    }

    std::vector<std::shared_ptr<Term>> newSubterms;
    newSubterms.reserve(subterms.size());
    bool changed = false;
    for (const auto &sub : subterms) {
      auto newSub = closeTerm(sub);
      if (newSub != sub) {
        changed = true;
      }
      newSubterms.push_back(newSub);
    }

    if (!changed) {
      return term;
    }
    return term->withSubterms(std::move(newSubterms));
  };

  for (auto &[varName, term] : substitution) {
    resolving.clear();
    resolving.insert(varName);
    substitution[varName] = closeTerm(term);
  }
}

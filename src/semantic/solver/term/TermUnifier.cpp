#include "TermUnifier.h"
#include "../../SemanticLogging.h"

void TermUnifier::addConstraint(std::shared_ptr<Term> t1,
                                std::shared_ptr<Term> t2) {
  SEMANTIC_LOG(3, "unification")
      << "enqueue lhs=" << t1->toString() << " rhs=" << t2->toString();
  constraints.emplace_back(std::move(t1), std::move(t2));
}

void TermUnifier::solve() {
  SEMANTIC_LOG(3, "unification")
      << "solve-start constraints=" << constraints.size();
  std::size_t i = 0;
  while (i < constraints.size()) {
    auto [t1, t2] = constraints[i];
    ++i;
    unify(t1, t2);
  }
  // Remove only the processed constraints, keeping any newly-added ones for
  // the next solve() call.
  if (i > 0) {
    constraints.erase(constraints.begin(),
                      constraints.begin() + static_cast<std::ptrdiff_t>(i));
  }
  SEMANTIC_LOG(3, "unification") << "solve-complete processed=" << i;
}

/**
 * Unify two terms using the union-find structure.
 *
 * Mirrors Unifier::unify():
 *   var + var      → quick_union (either can be root)
 *   var + proper   → quick_union(var, proper)  (proper type wins as root)
 *   proper + var   → quick_union(var, proper)
 *   cons + cons    → functor/arity check; quick_union; recurse on subterms
 *   otherwise      → throw TermUnificationError
 *
 * No occurs check is performed.  Cyclic bindings are resolved by
 * TopTermClosure::close() using the TopMu constructor.
 */
void TermUnifier::unify(std::shared_ptr<Term> t1, std::shared_ptr<Term> t2) {
  SEMANTIC_LOG(3, "unification")
      << "unify lhs=" << t1->toString() << " rhs=" << t2->toString();
  t1 = unionFind.find(t1);
  t2 = unionFind.find(t2);

  if (t1->equals(*t2)) {
    return;
  }

  if (t1->isVariable() && t2->isVariable()) {
    unionFind.quick_union(t1, t2);
  } else if (t1->isVariable() && !t2->isVariable()) {
    unionFind.quick_union(t1, t2); // proper type t2 becomes root
  } else if (!t1->isVariable() && t2->isVariable()) {
    unionFind.quick_union(t2, t1); // proper type t1 becomes root
  } else {
    // Both proper types
    if (!t1->matchesFunctor(*t2)) {
      throw TermUnificationError("Cannot unify " + t1->toString() + " with " +
                                     t2->toString() + ": different structure",
                                 t1, t2);
    }
    if (!t1->unifiesByStructureOnlyWith(*t2)) {
      if (t1->preferAsRepresentativeOver(*t2)) {
        unionFind.quick_union(t2, t1);
      } else {
        unionFind.quick_union(t1, t2);
      }
    }
    auto subs1 = t1->getSubterms();
    auto subs2 = t2->getSubterms();
    for (std::size_t i = 0; i < subs1.size(); ++i) {
      constraints.emplace_back(subs1[i], subs2[i]);
    }
  }
}

std::shared_ptr<Term> TermUnifier::find(std::shared_ptr<Term> term) {
  return unionFind.find(term);
}

std::shared_ptr<Term> TermUnifier::apply(std::shared_ptr<Term> term) {
  std::unordered_set<const Term *> active;
  return apply(term, active);
}

std::shared_ptr<Term>
TermUnifier::apply(std::shared_ptr<Term> term,
                   std::unordered_set<const Term *> &active) {
  auto rep = unionFind.find(term);

  if (rep->isVariable()) {
    return rep; // unbound variable
  }

  if (active.count(rep.get()) > 0) {
    return term;
  }

  active.insert(rep.get());

  auto subterms = rep->getSubterms();
  if (subterms.empty()) {
    active.erase(rep.get());
    return rep;
  }

  std::vector<std::shared_ptr<Term>> newSubterms;
  newSubterms.reserve(subterms.size());
  bool changed = false;
  for (const auto &sub : subterms) {
    auto newSub = apply(sub, active);
    if (!newSub->equals(*sub)) {
      changed = true;
    }
    newSubterms.push_back(newSub);
  }

  active.erase(rep.get());

  if (!changed) {
    return rep;
  }
  return rep->withSubterms(std::move(newSubterms));
}

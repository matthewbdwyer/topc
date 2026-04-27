#include "TipTermClosure.h"

#include "Copier.h"
#include "InternalError.h"
#include "Substituter.h"
#include "TipAlpha.h"
#include "TipCons.h"
#include "TipMu.h"
#include "TypeVars.h"
#include <memory>

// ---------------------------------------------------------------------------
// Local helpers (mirror Unifier's anonymous-namespace helpers)
// ---------------------------------------------------------------------------

namespace {

bool isVar(const std::shared_ptr<TipType> &type) {
  return std::dynamic_pointer_cast<TipVar>(type) != nullptr;
}

bool isAlpha(const std::shared_ptr<TipType> &type) {
  return std::dynamic_pointer_cast<TipAlpha>(type) != nullptr;
}

bool isCons(const std::shared_ptr<TipType> &type) {
  return std::dynamic_pointer_cast<TipCons>(type) != nullptr;
}

bool isMu(const std::shared_ptr<TipType> &type) {
  return std::dynamic_pointer_cast<TipMu>(type) != nullptr;
}

} // namespace

// ---------------------------------------------------------------------------
// TipTermClosure
// ---------------------------------------------------------------------------

TipTermClosure::TipTermClosure(const TermUnifier::Substitution &sub,
                               const TipVarRegistry &reg)
    : substitution(sub), registry(reg) {}

bool TipTermClosure::isBound(const std::string &key) const {
  return substitution.count(key) > 0;
}

/**
 * Follow the substitution chain starting from key.
 *
 * This mirrors TermUnifier::find(): follow variable→variable links until
 * either a non-variable term is reached or the key is absent from the
 * substitution (unbound).  We cast each hop to TipType (safe because every
 * term in the substitution was originally a TipType).
 */
std::shared_ptr<TipType> TipTermClosure::find(const std::string &key) const {
  auto it = substitution.find(key);
  if (it == substitution.end()) {
    return nullptr; // caller checks isBound before calling find
  }
  auto term = std::static_pointer_cast<TipType>(it->second);
  if (isVar(term)) {
    auto v = std::dynamic_pointer_cast<TipVar>(term);
    auto next = substitution.find(v->getFunctor());
    if (next != substitution.end() && next->second != it->second) {
      return find(v->getFunctor());
    }
  }
  return term;
}

/**
 * Close a type expression by replacing all bound variables with their
 * inferred types, wrapping cyclic bindings in TipMu.
 *
 * The algorithm is identical to Unifier::close() with the following
 * substitutions:
 *   unionFind->find(v) != v      →  isBound(key)
 *   close(unionFind->find(v),…)  →  close(find(key),…)  [one-hop; recursion
 *                                                         handles chains]
 *   unionFind->add(newTypes)     →  (omitted; no union-find to update)
 */
std::shared_ptr<TipType> TipTermClosure::close(std::shared_ptr<TipType> type,
                                               TipVarSet visited) {
  if (isVar(type)) {
    auto v = std::dynamic_pointer_cast<TipVar>(type);
    auto key = v->getFunctor();

    if (!visited.count(v) && isBound(key)) {
      // Bound variable — follow one hop then recurse
      visited.insert(v);

      auto bound = find(key);
      auto closedV = close(bound, visited);

      // Reuse existing alpha; otherwise create a fresh one for this node
      auto newV =
          isAlpha(v) ? v : std::make_shared<TipAlpha>(v->getNode());

      auto freeV = TypeVars::collect(closedV.get());

      if ((*closedV != *newV) && freeV.count(newV)) {
        // Cyclic reference — wrap in mu
        auto substClosedV =
            Substituter::substitute(closedV.get(), v.get(), newV);
        return std::make_shared<TipMu>(newV, substClosedV);
      } else {
        return closedV;
      }
    } else {
      // Unbound or already-visited (cycle guard) — return a fresh alpha
      return std::make_shared<TipAlpha>(v->getNode());
    }

  } else if (isCons(type)) {
    auto c = std::dynamic_pointer_cast<TipCons>(type);
    auto copy = Copier::copy(c);

    auto freeV = TypeVars::collect(c.get());

    std::vector<std::shared_ptr<TipType>> temp;
    auto current = c->getArguments();
    for (auto fv : freeV) {
      auto closedV = close(fv, visited);
      for (auto a : current) {
        auto subst = Substituter::substitute(a.get(), fv.get(), closedV);
        temp.push_back(subst);
      }
      current = temp;
      temp.clear();
    }

    auto consCopy = std::dynamic_pointer_cast<TipCons>(copy);
    consCopy->setArguments(current);
    return consCopy;

  } else if (isMu(type)) {
    auto m = std::dynamic_pointer_cast<TipMu>(type);
    return std::make_shared<TipMu>(m->getV(), close(m->getT(), visited));
  }

  // LCOV_EXCL_START
  throw InternalError("unreachable : type must be Var, Cons or Mu");
  // LCOV_EXCL_STOP
}

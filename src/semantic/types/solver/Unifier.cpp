#include "Unifier.h"

#include "TipAlpha.h"
#include "TipCons.h"
#include "TipMu.h"
#include "UnificationError.h"

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

bool Unifier::isCons(std::shared_ptr<TipType> type) {
  return std::dynamic_pointer_cast<TipCons>(type) != nullptr;
}

bool Unifier::isMu(std::shared_ptr<TipType> type) {
  return std::dynamic_pointer_cast<TipMu>(type) != nullptr;
}

bool Unifier::isVar(std::shared_ptr<TipType> type) {
  return std::dynamic_pointer_cast<TipVar>(type) != nullptr;
}

bool Unifier::isAlpha(std::shared_ptr<TipType> type) {
  return std::dynamic_pointer_cast<TipAlpha>(type) != nullptr;
}

bool Unifier::isProperType(std::shared_ptr<TipType> type) {
  return isCons(type) || isMu(type) || isAlpha(type);
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

Unifier::Unifier() = default;

Unifier::Unifier(std::vector<TypeConstraint> constraints) {
  for (auto const &c : constraints) {
    termUnifier.addConstraint(TipTermAdapter::wrap(c.lhs),
                              TipTermAdapter::wrap(c.rhs));
  }
}

// ---------------------------------------------------------------------------
// Mutation
// ---------------------------------------------------------------------------

void Unifier::add(std::vector<TypeConstraint> constraints) {
  for (auto const &c : constraints) {
    termUnifier.addConstraint(TipTermAdapter::wrap(c.lhs),
                              TipTermAdapter::wrap(c.rhs));
  }
}

void Unifier::solve() {
  try {
    termUnifier.solve();
  } catch (const TermUnificationError &e) {
    throw UnificationError(e.what());
  }
}

/**
 * Unify two types immediately (on-the-fly).
 *
 * Mirrors Unifier::unify(): adds one constraint and solves it right away so
 * that subsequent calls to inferred() see the updated union-find state.
 *
 * Translates TermUnificationError → UnificationError so callers that catch
 * UnificationError continue to work without changes.
 */
void Unifier::unify(std::shared_ptr<TipType> t1,
                    std::shared_ptr<TipType> t2) {
  termUnifier.addConstraint(TipTermAdapter::wrap(t1),
                            TipTermAdapter::wrap(t2));
  try {
    termUnifier.solve();
  } catch (const TermUnificationError &e) {
    throw UnificationError(e.what());
  }
}

// ---------------------------------------------------------------------------
// Inference
// ---------------------------------------------------------------------------

std::shared_ptr<TipType> Unifier::inferred(std::shared_ptr<TipType> t) {
  return TipTypeClosure(termUnifier).close(t);
}

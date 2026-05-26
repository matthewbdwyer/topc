#include "Unifier.h"

#include "TopAlpha.h"
#include "TopCons.h"
#include "TopMu.h"
#include "UnificationError.h"

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

bool Unifier::isCons(std::shared_ptr<TopType> type) {
  return std::dynamic_pointer_cast<TopCons>(type) != nullptr;
}

bool Unifier::isMu(std::shared_ptr<TopType> type) {
  return std::dynamic_pointer_cast<TopMu>(type) != nullptr;
}

bool Unifier::isVar(std::shared_ptr<TopType> type) {
  return std::dynamic_pointer_cast<TopVar>(type) != nullptr;
}

bool Unifier::isAlpha(std::shared_ptr<TopType> type) {
  return std::dynamic_pointer_cast<TopAlpha>(type) != nullptr;
}

bool Unifier::isProperType(std::shared_ptr<TopType> type) {
  return isCons(type) || isMu(type) || isAlpha(type);
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

Unifier::Unifier() = default;

Unifier::Unifier(std::vector<TypeConstraint> constraints) {
  for (auto const &c : constraints) {
    termUnifier.addConstraint(TopTermAdapter::wrap(c.lhs),
                              TopTermAdapter::wrap(c.rhs));
  }
}

// ---------------------------------------------------------------------------
// Mutation
// ---------------------------------------------------------------------------

void Unifier::add(std::vector<TypeConstraint> constraints) {
  for (auto const &c : constraints) {
    termUnifier.addConstraint(TopTermAdapter::wrap(c.lhs),
                              TopTermAdapter::wrap(c.rhs));
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
void Unifier::unify(std::shared_ptr<TopType> t1,
                    std::shared_ptr<TopType> t2) {
  termUnifier.addConstraint(TopTermAdapter::wrap(t1),
                            TopTermAdapter::wrap(t2));
  try {
    termUnifier.solve();
  } catch (const TermUnificationError &e) {
    throw UnificationError(e.what());
  }
}

// ---------------------------------------------------------------------------
// Inference
// ---------------------------------------------------------------------------

std::shared_ptr<TopType> Unifier::inferred(std::shared_ptr<TopType> t) {
  return TopTypeClosure(termUnifier).close(t);
}

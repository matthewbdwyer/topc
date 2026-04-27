#include "TipTermBridge.h"

#include "TipAlpha.h"
#include "TipCons.h"
#include "TipMu.h"
#include "UnificationError.h"

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

bool TipTermBridge::isCons(std::shared_ptr<TipType> type) {
  return std::dynamic_pointer_cast<TipCons>(type) != nullptr;
}

bool TipTermBridge::isMu(std::shared_ptr<TipType> type) {
  return std::dynamic_pointer_cast<TipMu>(type) != nullptr;
}

bool TipTermBridge::isVar(std::shared_ptr<TipType> type) {
  return std::dynamic_pointer_cast<TipVar>(type) != nullptr;
}

bool TipTermBridge::isAlpha(std::shared_ptr<TipType> type) {
  return std::dynamic_pointer_cast<TipAlpha>(type) != nullptr;
}

bool TipTermBridge::isProperType(std::shared_ptr<TipType> type) {
  return isCons(type) || isMu(type) || isAlpha(type);
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TipTermBridge::TipTermBridge() = default;

TipTermBridge::TipTermBridge(std::vector<TypeConstraint> constraints)
    : pendingConstraints(std::move(constraints)) {
  for (auto const &c : pendingConstraints) {
    termUnifier.addConstraint(c.lhs, c.rhs);
  }
}

// ---------------------------------------------------------------------------
// Mutation
// ---------------------------------------------------------------------------

void TipTermBridge::add(std::vector<TypeConstraint> constraints) {
  for (auto const &c : constraints) {
    pendingConstraints.push_back(c);
    termUnifier.addConstraint(c.lhs, c.rhs);
  }
}

void TipTermBridge::solve() {
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
void TipTermBridge::unify(std::shared_ptr<TipType> t1,
                          std::shared_ptr<TipType> t2) {
  termUnifier.addConstraint(t1, t2);
  try {
    termUnifier.solve();
  } catch (const TermUnificationError &e) {
    throw UnificationError(e.what());
  }
}

// ---------------------------------------------------------------------------
// Inference
// ---------------------------------------------------------------------------

std::shared_ptr<TipType> TipTermBridge::inferred(std::shared_ptr<TipType> t) {
  return TipTermClosure(termUnifier).close(t);
}

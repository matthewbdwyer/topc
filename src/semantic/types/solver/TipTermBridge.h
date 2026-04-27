#pragma once

#include "TipTermClosure.h"
#include "TipType.h"
#include "TipVar.h"
#include "TermUnifier.h"
#include "TypeConstraint.h"
#include <memory>
#include <vector>

/*!
 * \class TipTermBridge
 *
 * \brief Drop-in replacement for Unifier that delegates to TermUnifier.
 *
 * TipTermBridge provides the same public API as Unifier but uses TermUnifier
 * (backed by TermUnionFind) internally.  This allows TipTermBridge to be
 * substituted for Unifier everywhere in the codebase without changing callers,
 * while ensuring the type-inference algorithm operates over the generic Term
 * abstraction layer.
 *
 * Static helper predicates (isCons, isMu, isVar, isAlpha, isProperType)
 * mirror Unifier::* exactly.
 */
class TipTermBridge {
public:
  TipTermBridge();
  explicit TipTermBridge(std::vector<TypeConstraint> constraints);
  ~TipTermBridge() = default;

  /*!
   * \brief Attempt to unify two types immediately (on-the-fly mode).
   * \throws UnificationError when constraints cannot be unified.
   *
   * Wraps the constraint in a TermUnifier call and solves it inline,
   * mirroring Unifier::unify().
   */
  void unify(std::shared_ptr<TipType> t1, std::shared_ptr<TipType> t2);

  /*! \brief Add constraints to this bridge (mirrors Unifier::add()). */
  void add(std::vector<TypeConstraint> constraints);

  /*!
   * \brief Solve all pending constraints (batch mode).
   * \pre addConstraint calls have been made or constructor seeded with
   *      constraints.
   */
  void solve();

  /*!
   * \brief Return the inferred type for a given type variable.
   *
   * Closes the type by replacing all bound variables with their inferred
   * proper types, producing TipMu for recursive types.
   */
  std::shared_ptr<TipType> inferred(std::shared_ptr<TipType> t);

  static bool isCons(std::shared_ptr<TipType> type);
  static bool isMu(std::shared_ptr<TipType> type);
  static bool isVar(std::shared_ptr<TipType> type);
  static bool isAlpha(std::shared_ptr<TipType> type);
  static bool isProperType(std::shared_ptr<TipType> type);

private:
  TermUnifier termUnifier;
  std::vector<TypeConstraint> pendingConstraints;
};

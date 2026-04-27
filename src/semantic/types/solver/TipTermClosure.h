#pragma once

#include "TermUnifier.h"
#include "TipType.h"
#include "TipVar.h"
#include "TipVarRegistry.h"
#include <memory>

/**
 * @class TipTermClosure
 * @brief Closes TipType expressions by reading from a TermUnifier substitution.
 *
 * This mirrors the logic of Unifier::close() but sources variable bindings
 * from the flat substitution map produced by TermUnifier::getSubstitution()
 * instead of from a union-find structure.  Cycle detection is handled
 * identically via the TipVarSet visited parameter and produces TipMu types.
 */
class TipTermClosure {
  const TermUnifier::Substitution &substitution;
  const TipVarRegistry &registry;

  /** Follow the substitution chain from key, returning the terminal term. */
  std::shared_ptr<TipType> find(const std::string &key) const;

  /** True if key has a direct binding in the substitution. */
  bool isBound(const std::string &key) const;

public:
  TipTermClosure(const TermUnifier::Substitution &substitution,
                 const TipVarRegistry &registry);

  /**
   * @brief Close a type by replacing all bound variables with their inferred
   *        types.  Cyclic bindings produce TipMu quantifiers.
   */
  std::shared_ptr<TipType> close(std::shared_ptr<TipType> type,
                                 TipVarSet visited = {});
};

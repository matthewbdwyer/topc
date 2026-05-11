#pragma once

#include "TipTermAdapter.h"
#include "TermUnifier.h"
#include "TipType.h"
#include "TipVar.h"
#include <memory>

/**
 * @class TipTypeClosure
 * @brief Closes TipType expressions using a TermUnifier's union-find solution.
 *
 * Mirrors Unifier::close() but reads variable bindings via
 * TermUnifier::find() instead of a union-find on TipType objects directly.
 * Cycle detection is handled via the TipVarSet visited parameter and produces
 * TipMu quantifiers, identical to Unifier::close().
 */
class TipTypeClosure {
  const TermUnifier &unifier;

public:
  explicit TipTypeClosure(const TermUnifier &unifier);

  /**
   * @brief Close a type by replacing all bound variables with their inferred
   *        types.  Cyclic bindings produce TipMu quantifiers.
   */
  std::shared_ptr<TipType> close(std::shared_ptr<TipType> type,
                                 TipVarSet visited = {});
};

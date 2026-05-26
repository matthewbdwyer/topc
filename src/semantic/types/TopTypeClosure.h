#pragma once

#include "TopTermAdapter.h"
#include "TermUnifier.h"
#include "TopType.h"
#include "TopVar.h"
#include <memory>

/**
 * @class TopTypeClosure
 * @brief Closes TopType expressions using a TermUnifier's union-find solution.
 *
 * Mirrors Unifier::close() but reads variable bindings via
 * TermUnifier::find() instead of a union-find on TopType objects directly.
 * Cycle detection is handled via the TopVarSet visited parameter and produces
 * TopMu quantifiers, identical to Unifier::close().
 */
class TopTypeClosure {
  const TermUnifier &unifier;

public:
  explicit TopTypeClosure(const TermUnifier &unifier);

  /**
   * @brief Close a type by replacing all bound variables with their inferred
   *        types.  Cyclic bindings produce TopMu quantifiers.
   */
  std::shared_ptr<TopType> close(std::shared_ptr<TopType> type,
                                 TopVarSet visited = {});
};

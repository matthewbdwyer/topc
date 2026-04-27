#pragma once

#include "TipVar.h"
#include <map>
#include <memory>
#include <string>

/**
 * @class TipVarRegistry
 * @brief Maps TipVar functor keys to their originating shared_ptr<TipVar>.
 *
 * The registry enables recovery of the original TipVar (or TipAlpha) pointer
 * from the string key used in TermUnifier's substitution map.  It is
 * populated at constraint-collection time and then consulted by
 * TipTermClosure when closing types against a TermUnifier substitution.
 */
class TipVarRegistry {
  std::map<std::string, std::shared_ptr<TipVar>> registry;

public:
  /**
   * @brief Register a TipVar using its functor as the key.
   *
   * Idempotent: registering the same variable twice has no effect.
   */
  void register_(std::shared_ptr<TipVar> var);

  /**
   * @brief Look up a TipVar by its functor key.
   * @return The registered TipVar, or nullptr if the key is unknown.
   */
  std::shared_ptr<TipVar> lookup(const std::string &key) const;
};

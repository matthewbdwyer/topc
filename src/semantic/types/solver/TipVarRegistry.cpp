#include "TipVarRegistry.h"

void TipVarRegistry::register_(std::shared_ptr<TipVar> var) {
  registry.emplace(var->getFunctor(), var);
}

std::shared_ptr<TipVar> TipVarRegistry::lookup(const std::string &key) const {
  auto it = registry.find(key);
  return it != registry.end() ? it->second : nullptr;
}

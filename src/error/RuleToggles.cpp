#include "RuleToggles.h"

#include <algorithm>
#include <cstdlib>
#include <set>
#include <sstream>

namespace {

std::set<std::string> &disabled() {
  static std::set<std::string> ids;
  return ids;
}

} // namespace

namespace RuleToggles {

const std::vector<std::string> &known() {
  static const std::vector<std::string> ids = {
      // Checks: each rejects a class of programs.
      "borrow-position",        // &x only as a call argument (syntactic)
      "borrow-escape",          // borrow-derived value reaches a sink
      "borrow-component",       // no borrow inside a payload, alloc, or result
      "alloc-payload",          // an owning pointer's payload is Copy
      "alias-binder",           // binder of case *e may only be reborrowed
      "alias-move-out",         // *p of an owned referent is not taken
      "alias-overwrite",        // *p = v may not replace an owned referent
      "alias-undecidable",      // generic deref not through a formal
      "use-after-move",         // use, borrow, or second move after a move
      "assign-over-live",       // assigning an owner that still owns
      "call-held-borrow",       // a call may not move an owner it borrows
      "join-agreement",         // owned on every path or on none
      "loop-body-invariant",    // a loop body leaves ownership unchanged
      "loop-condition",         // a loop condition moves nothing
      "generic-copy-bound",     // requirement violated at a call
      "generic-untracked-actual", // requirement cannot move to a caller
      "generic-drop",           // owned value to a generic formal that drops it
      "recursive-type",         // recursive function types unsupported
      // Mechanisms: each makes accepted programs memory-safe.
      "destroy-at-exit",        // free owners still owned at function exit
      "destroy-arm-binders",    // free by-value case binders at arm end
      "free-unbound-reference", // free *mk() after the read or write
      "division-check",         // divide by zero / overflow is a runtime error
  };
  return ids;
}

bool disable(const std::string &id) {
  const auto &ids = known();
  if (std::find(ids.begin(), ids.end(), id) == ids.end()) {
    return false;
  }
  disabled().insert(id);
  return true;
}

std::string disableList(const std::string &list) {
  std::stringstream stream(list);
  std::string id;
  while (std::getline(stream, id, ',')) {
    if (id.empty()) {
      continue;
    }
    if (!disable(id)) {
      return id;
    }
  }
  return "";
}

bool enabled(const char *id) {
  // The environment variable reaches every process that runs the analyses,
  // including the unit tests, not only topc's command line.
  static bool fromEnvironment = [] {
    if (const char *env = std::getenv("TOPC_UNSAFE_DISABLE")) {
      disableList(env);
    }
    return true;
  }();
  (void)fromEnvironment;
  return disabled().count(id) == 0;
}

} // namespace RuleToggles

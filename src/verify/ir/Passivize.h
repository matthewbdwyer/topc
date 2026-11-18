#pragma once

#include "Proc.h"

namespace verutop {

// ===========================================================================
// Passivization, after Flanagan and Saxe (POPL 2001).
//
// 1. Version every variable: x := e becomes assume x@k == e for a fresh
//    version x@k, so the program no longer assigns anything ("passive").
// 2. havoc x just starts a new version, with nothing assumed about it.
// 3. After a choice, each branch assumes the merged version equals its own,
//    so the code after the join names one version.
// ===========================================================================

struct Passive {
  G prog;
  std::vector<std::string> versions; // declared constants
  std::map<std::string, std::string> sorts;
  std::vector<std::string> entry; // the formals' entry versions
};

Passive passivize(const Proc &p);

} // namespace verutop

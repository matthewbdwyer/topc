#pragma once

#include "Gcl.h"

namespace verutop {

// ===========================================================================
// A proof outline: each command preceded by its weakest precondition, with
// substitution carried out and the result simplified.
// ===========================================================================

// A formula in braces, as in a Hoare triple; long ones are cut off.
std::vector<std::string> braces(const T &P);

struct Outline {
  T pre;
  std::vector<std::string> lines;
};
Outline outline(const G &c, const T &Q, int ind = 0);

} // namespace verutop

#pragma once

#include "Gcl.h"
#include "Passivize.h"
#include "Query.h"
#include "Solver.h"

#include <map>

namespace verutop {

// ===========================================================================
// Counterexample traces.  Z3's model gives a value to every version; the
// trace follows the passive program along the path the model takes.
// ===========================================================================

std::string baseName(const std::string &version);

// The arguments of a counterexample, as "x = -1, y = 0"; if the solver gave
// no values, whatever it said instead, on one line.
std::string counterexample(const Answer &a, const std::vector<std::string> &entry);

// The failing execution, line by line: the entry values, each assignment
// with its value, each branch taken, and the assertion that fails with the
// values substituted.  `asked` is the trace query and `a` its answer.
std::vector<std::string> trace(const Passive &pv, const Query &asked, const Answer &a, int target);

} // namespace verutop

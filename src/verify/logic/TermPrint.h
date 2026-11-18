#pragma once

#include "Term.h"

namespace verutop {

// A term as SMT-LIB text, for Z3.
void smt(const T &t, std::string &o);
std::string smt(const T &t);

// A term in infix, for people: on one line, or broken at its top-level
// connectives when wider than `width`.
std::string flat(const T &t);
std::vector<std::string> pretty(const T &t, int width);

} // namespace verutop

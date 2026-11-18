#pragma once

#include "Passivize.h"
#include "VcStrategy.h"

namespace verutop {

extern const char *PRELUDE;

// For asking Z3 what a proof depends on.  Each tracked assumption P becomes
// A!k ==> P, and each other obligation Q (which this proof may assume)
// O!j ==> Q; the flags are then asserted by name, and the names in Z3's
// unsat core are the ones the proof needed.
//
// With `reach`, the target obligation becomes `false`: if that is proved,
// no execution reaches the obligation at all, and its proof says nothing.
G flagged(const G &c, int target, bool reach);

// The declarations, definitions and negated VC of one passive query.
std::string passiveQuery(const Passive &pv, const PassiveVC &vc);

} // namespace verutop

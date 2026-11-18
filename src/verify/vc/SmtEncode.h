#pragma once

#include "Passivize.h"
#include "Query.h"
#include "VcStrategy.h"

namespace verutop {

// ===========================================================================
// From a function's VC to the queries asked about it.
// ===========================================================================

// Is obligation `target` valid?  If not, the entry values are wanted.
Query obligationQuery(const Vc &vc);

// The same question about the passive program, asking for the value of
// every version and every branch condition: enough to replay the failing
// execution as a trace.
Query traceQuery(const Passive &pv, int target);

// For asking Z3 what a proof depends on.  Each tracked assumption P becomes
// A!k ==> P, and each other obligation Q (which this proof may assume)
// O!j ==> Q; the flags are then asserted by name, and the names in Z3's
// unsat core are the ones the proof needed.
//
// With `reach`, the target obligation becomes `false`: if that is proved,
// no execution reaches the obligation at all, and its proof says nothing.
G flagged(const G &c, int target, bool reach);
Query dependencyQuery(const Proc &proc, const Passive &pv, int target, bool reach);

} // namespace verutop

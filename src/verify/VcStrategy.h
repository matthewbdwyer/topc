#pragma once

#include "Lowering.h"

namespace verutop {

// ===========================================================================
// Verification conditions.  `target` is the obligation checked; every other
// obligation is assumed.  Target -2 checks them all (for --pvc and --pwp).
// ===========================================================================

// Whether this assertion is one the VC checks (and does not assume).
bool checked(const Assert &a, int target);

// wp by substitution, over guarded commands; then for the whole function,
// with the entry values of the formals.
T wp(const G &c, const T &Q, int target);
T wpFunction(const Proc &p, int target);

// The VC of a passive command needs no substitution.  For each command:
// ok (its assertions hold) and post (what holds when it finishes).
//   ok(S1; S2) = ok1 && (post1 ==> ok2)      post(S1; S2) = post1 && post2
//   ok(S1 [] S2) = ok1 && ok2                post(S1 [] S2) = post1 || post2
// Each post is named with a define-fun, so it is written once and the VC
// grows linearly with the program.
struct PassiveVC {
  std::vector<std::pair<std::string, T>> defs;
  T ok;
};
PassiveVC passiveVc(const G &prog, int target);

} // namespace verutop

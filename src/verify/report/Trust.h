#pragma once

#include "Results.h"

#include <set>
#include <string>
#include <vector>

namespace verutop {

// ===========================================================================
// What the proofs rest on.  Computed from the results alone: no queries,
// no printing.
// ===========================================================================

struct FunctionTrust {
  std::string name;
  std::vector<std::string> lines; // per obligation: what its proof uses, or why it is vacuous
  std::string verdict;            // verified / verified, assuming: ... / not verified: ...
};

struct TrustReport {
  std::vector<std::string> assumed;  // used by some proof and not established in this run
  std::vector<std::string> diverges; // `decreases *`: not shown to terminate
  std::vector<std::string> checked;  // things other tools take on trust, proved here
  bool anyVacuous = false;
  std::vector<FunctionTrust> functions; // meaningful when dependencies were asked for (--ptrust)
};

TrustReport analyzeTrust(const VerifyResult &res, bool trustSpecs);

// Whether a proof that nothing reaches an obligation makes its real proof
// vacuous.  It does not when the unreachability rests on an obligation that
// failed: every later obligation assumes the earlier ones, so a failure
// (say, a loop without `decreases`, or `assert false`) cuts off what
// follows.  That failure is already reported; marking the rest VACUOUS
// would only repeat it.
bool vacuous(const std::vector<std::string> &reachCore, const std::set<int> &unprovedObligations);

} // namespace verutop

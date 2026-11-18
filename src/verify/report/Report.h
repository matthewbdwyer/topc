#pragma once

#include "Results.h"
#include "Trust.h"
#include "VCGen.h"

#include <ostream>
#include <string>

namespace verutop {

// ===========================================================================
// Presentation only.  Each view is the printer for one IR:
//
//   --pspec     spec functions: definitions, axioms, termination obligations
//   --pgcl      guarded commands
//   --ppassive  passive commands
//   --pwp       the proof outline over guarded commands
//   --pvc       the verification condition, by the chosen strategy
//   --psmt      queries (and, with --ptrust, the dependency queries)
//   (always)    verdicts, with counterexamples and --pcex traces
//   (always)    the trust summary; with --ptrust, the trust report
// ===========================================================================

// Writes every query as one readable SMT-LIB file; returns its size.
long long writeSmtFile(const std::string &path, const VerifyResult &res);

void printReport(std::ostream &out, const VerifyResult &res, const TrustReport &trust,
                 const VCGen::Options &opt, const std::string &smtFile, long long smtBytes,
                 const std::string &solverVersion);

} // namespace verutop

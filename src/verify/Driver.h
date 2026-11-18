#pragma once

#include "Results.h"
#include "Solver.h"
#include "VCGen.h"

class ASTProgram;
class TypeInference;

namespace verutop {

// Runs the pipeline on a checked program and returns what it found.  It
// prints nothing, and the only thing it knows about the solver is the
// interface.
VerifyResult verify(ASTProgram *program, TypeInference *types, Solver &solver, const VCGen::Options &opt);

} // namespace verutop

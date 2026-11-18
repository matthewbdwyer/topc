#pragma once

#include <ostream>
#include <string>

class ASTProgram;
class TypeInference;

/*! \brief VERUTOP: verification-condition generation for TOP.
 *
 * Lowers each function to guarded commands, computes a verification
 * condition from them (by substitution or, after passivization, by Flanagan
 * and Saxe's method), writes one SMT-LIB query per proof obligation, and asks
 * Z3 whether each can fail.  Each stage can be printed.  Returns true if
 * every obligation is proved.
 */
namespace VCGen {
// The VC is always a weakest precondition; this is how it is computed.
enum class VcMode {
  Subst,       // --vcmode=subst: by substitution, over the guarded commands as lowered
  PassiveCopy, // --vcmode=passive-copy: of the passive program, the rest of the function
               // still copied under each branch
  Passive      // --vcmode=passive (default): of the passive program, the rest written once
};
struct Options {
  VcMode mode = VcMode::Passive;
  bool trustSpecs = false;
  long long rlimit = 20000000; // --vrlimit: Z3's resource limit for each query
  int jobs = 0;           // --vjobs: Z3 processes at once (0: one per core, at most 4)
  bool overflow = false;  // --overflow: + - * / in code must stay in 64 bits
  bool pgcl = false;      // guarded commands
  bool ppassive = false;  // passive (versioned) commands
  bool pwp = false;       // proof outline: wp before each command
  bool pvc = false;       // each function's VC, in infix
  bool pspec = false;     // spec-function definitions and termination
  bool pcex = false;      // a trace for each failure
  bool ptrust = false;    // what each proof depends on, and what is trusted
  bool psmt = false;      // the SMT-LIB queries
  int psmtLine = 0;       //   only those for this line (0: all)
  std::string fn;         // only this function (empty: all)
};

//! Reject a spec function used in code (both with and without --verify).
void checkSpecUse(ASTProgram *program);
bool verify(ASTProgram *program, TypeInference *types, const std::string &smtFile,
            std::ostream &out, const Options &opt);
} // namespace VCGen

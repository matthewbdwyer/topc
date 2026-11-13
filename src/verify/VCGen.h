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
struct Options {
  bool passive = true;    // --vcmode=passive (default) or wp
  bool trustSpecs = false;
  bool pgcl = false;      // guarded commands
  bool ppassive = false;  // passive (versioned) commands
  bool pwp = false;       // proof outline: wp before each command
  bool pvc = false;       // each function's VC, in infix
  bool pspec = false;     // spec-function definitions and termination
  bool pcex = false;      // a trace for each failure
  bool psmt = false;      // the SMT-LIB queries
  int psmtLine = 0;       //   only those for this line (0: all)
  std::string fn;         // only this function (empty: all)
};

//! Reject a spec function used in code (both with and without --verify).
void checkSpecUse(ASTProgram *program);
bool verify(ASTProgram *program, TypeInference *types, const std::string &smtFile,
            std::ostream &out, const Options &opt);
} // namespace VCGen

#pragma once

#include <string>

namespace verutop {

// What a proof obligation claims.  Its message is derived from the kind
// (and the subject), so nothing downstream has to recognise strings.
enum class ObKind {
  Postcondition,      // ensures holds at the return
  Precondition,       // the callee's requires holds at a call; subject: the call
  Assertion,          // an assert statement
  InvariantOnEntry,   // a loop invariant holds before the loop
  InvariantPreserved, // and after an arbitrary iteration
  LoopMeasure,        // the loop's decreases expression gets smaller and stays >= 0
  CallMeasure,        // a recursive call makes the function's measure smaller
  MutualCallMeasure,  // the same, for a call around a cycle of functions
  DivisorNotZero,
  NoOverflow,       // --overflow: the result is a 64-bit value; subject: the expression
  ErrorUnreachable, // no execution reaches an error statement
  BorrowsDisjoint,  // two borrows of one variable, one of them written
  // A clause the program lacks.  These fail without asking Z3, and nothing
  // after them assumes them: they are not facts about the program.
  MissingLoopMeasure,       // a loop with no decreases
  MissingCallMeasure,       // a recursive call, and the function has no decreases
  MissingMutualCallMeasure, // subject: the functions on the cycle without one
  LoopMayNotTerminate,      // a `decreases *` loop; subject: the function that must say so too
  CallMayNotTerminate       // a call to a `decreases *` function; subject as above
};

struct Obligation {
  ObKind kind = ObKind::Assertion;
  int line = 0;
  int column = 0;
  std::string subject; // see ObKind
  std::string callee;  // Precondition, the call measures, CallMayNotTerminate

  // The obligation as the report names it.
  std::string what() const;
  // A clause the program lacks: fails without asking Z3.
  bool missing() const;
  // The claim is `false`: that no execution gets here.  Proving it is the
  // point, so such an obligation is never called vacuous.
  bool unreachable() const;
};

// Something a proof may use that the function itself does not establish.
// Each is tracked through the VC, so a proof can say which it needed.
struct Assumption {
  enum Kind {
    Pre,      // the function's own requires: the caller's obligation
    Assume,   // an assume statement: trusted
    Contract, // a callee's postcondition: holds if the callee is verified
    Lemma     // a lemma's postcondition, from `use`: likewise
  } kind;
  int line;
  std::string what;   // as shown in a report
  std::string callee; // Contract, Lemma
};

} // namespace verutop

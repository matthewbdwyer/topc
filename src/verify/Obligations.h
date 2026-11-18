#pragma once

#include <string>

namespace verutop {

struct Obligation {
  int line;
  std::string what;
  bool missing = false; // a clause the program lacks: fails without asking Z3
  bool unreachable = false; // the obligation is `false`: that no execution gets here
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

#pragma once

#include "Term.h"

#include <ostream>

namespace verutop {

// ===========================================================================
// Guarded commands.
// ===========================================================================

struct G {
  enum Kind { Assume, Assert, Assign, Havoc, Seq, Choice } kind = Seq;
  T p;                            // Assume, Assert: the formula; Assign: the value
  std::string x;                  // Assign: the variable
  std::vector<std::string> vars;  // Havoc
  std::vector<std::string> sorts; // Havoc: the sort of each variable
  std::vector<G> kids;            // Seq, Choice
  int id = -1;                    // Assert: obligation number
  int line = 0;
  std::string note;               // shown as a comment
  int asmId = -1;                 // Assume: the tracked assumption it states, if any
  bool guard = false;             // Assume: the condition that picks a branch
  bool missing = false;           // Assert: a clause the program lacks; it fails,
                                  // but nothing after it assumes it
  T src; // a guard: the source condition; a passive assignment: the value
};

G gAssume(T p, int line, std::string note, bool guard = false);
G gAssign(std::string x, T e, int line, std::string note = "");
// A sequence, with nested sequences flattened.
G gSeq(const std::vector<G> &ks);
G gChoice(G a, G b, int line);

// One primitive command as text lines (a formula may take several).
std::vector<std::string> showCmd(const G &c, bool passive, int ind = 0);
void printG(const G &c, int ind, std::ostream &o, bool passive);

} // namespace verutop

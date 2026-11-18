#pragma once

#include "Proc.h"
#include "Obligations.h"
#include "Passivize.h"
#include "Query.h"
#include "Solver.h"
#include "Termination.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

namespace verutop {

// ===========================================================================
// What a run of the verifier found, as data.  The driver fills it in;
// Trust reads it to say what the proofs rest on; Report prints it.  Nothing
// here is text for the user except what the solver or the program supplied.
// ===========================================================================

struct ObligationResult {
  Obligation ob;
  int index = -1;             // among its function's obligations
  Query query;                // what was asked
  Answer answer;              // and what the solver said
  std::string counterexample; // not proved: the arguments, "x = -1, y = 0", or the solver's message
  std::vector<std::string> trace; // --pcex: the failing execution, line by line

  // What the proof depends on (Query::flags, Axiom::name): asm!k, ob!j,
  // def!f, rank!T.  Known only if the solver could say.
  bool usesKnown = false;
  std::vector<std::string> uses;
  // Proved, but no execution reaches it: `uses` is then why not.
  bool vacuous = false;
  // The questions asked to find that out, in order.
  struct Asked {
    bool reach; // "does anything reach it?", or else "what does the proof use?"
    Query query;
  };
  std::vector<Asked> asked;

  bool proved() const { return answer.verdict == Verdict::Unsat; }
};

struct SpecResult {
  std::string name;
  T lhs, rhs;              // name(params) == body
  Axiom axiom;
  bool recursive = false;  // it has termination checks
  bool terminates = true;  // and they all passed
  bool used = true;        // its definition is available to proofs
};

struct FunctionResult {
  std::string name;
  bool ran = false;
  std::string skipped; // why not, if the prototype cannot handle it
  Proc proc;           // guarded commands, obligations, assumptions
  Passive passive;
  std::vector<ObligationResult> obligations;

  bool verified() const {
    if (!ran) return false;
    for (auto &r : obligations)
      if (!r.proved()) return false;
    return true;
  }
};

struct VerifyResult {
  std::vector<std::string> rejected; // spec functions the prototype cannot handle
  std::vector<SpecResult> specs;     // by name
  // Termination checks of spec functions: (function, result), in order.
  std::vector<std::pair<std::string, ObligationResult>> specChecks;
  std::vector<FunctionResult> functions;  // checked or skipped, in program order
  std::map<std::string, bool> rankProved; // rank#D -> its induction step was proved
  std::map<std::string, bool> hasEnsures; // every function of the program
  CallEdges calls;                        // caller -> callee, in code and lemmas
  Background background;

  const FunctionResult *function(const std::string &name) const {
    for (auto &f : functions)
      if (f.name == name) return &f;
    return nullptr;
  }
  // Every obligation asked was proved.
  bool ok() const {
    for (auto &[fn, r] : specChecks)
      if (!r.proved()) return false;
    for (auto &f : functions)
      for (auto &r : f.obligations)
        if (!r.proved()) return false;
    return true;
  }
};

// A function's results in the order the report lists them: by line.
inline std::vector<const ObligationResult *> byLine(const FunctionResult &f) {
  std::vector<const ObligationResult *> rs;
  for (auto &r : f.obligations) rs.push_back(&r);
  std::stable_sort(rs.begin(), rs.end(), [](auto *a, auto *b) { return a->ob.line < b->ob.line; });
  return rs;
}

} // namespace verutop

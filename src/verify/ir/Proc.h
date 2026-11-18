#pragma once

#include "Gcl.h"
#include "Obligations.h"

#include <map>
#include <string>
#include <vector>

class ASTFunction;

namespace verutop {

// A function in the verifier's IR: its guarded commands, the obligations
// they assert, and the assumptions a proof of them may use.  Lowering
// produces it; passivization, the VC strategies and the report consume it.
struct Proc {
  ASTFunction *fn = nullptr;
  std::vector<std::string> formals; // in order
  G body;
  std::vector<Obligation> obs;
  std::vector<Assumption> assumptions;
  std::vector<std::string> diverges;        // what `decreases *` leaves unproved
  int arithmetic = 0;                       // + - * / in code that no obligation checks
  std::map<std::string, std::string> sorts; // every variable's sort
  std::string sortOf(const std::string &x) const {
    auto it = sorts.find(x);
    return it == sorts.end() ? "Int" : it->second;
  }
};

} // namespace verutop

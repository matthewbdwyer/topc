#pragma once

#include "Term.h"

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace verutop {

// ===========================================================================
// A query is data: what is declared, what is assumed, what is asked.  Text
// is produced in one place (render), and what a query depends on is found
// from the symbols in its terms, not by searching that text.
// ===========================================================================

using Decl = std::pair<std::string, std::string>; // (name, sort)

// The definition of a function symbol: a spec function, or rank#D.  It is
// stated as a quantified equation, triggered on each application, so Z3
// unfolds the definition exactly where a proof mentions the function.
struct Axiom {
  std::string symbol;
  std::vector<Decl> params; // empty: a constant, and the body is asserted as is
  T trigger;                // the application symbol(params)
  T body;

  // The name it is asserted under when a proof's dependencies are wanted.
  std::string name() const;
};

// What every query of one program shares.
struct Background {
  long long rlimit = 0;  // Z3's resource limit for a query
  std::string sorts;     // datatypes, and the rank functions' declarations
  std::string functions; // declarations of the spec functions
  bool overflow = false; // INT_MIN and INT_MAX are defined
  // The definitions a query may use: rank functions always; a spec
  // function only once its termination check has passed.
  std::map<std::string, Axiom> axioms;
};

struct Query {
  // Bool constants asserted true by name (constant, name): the handles by
  // which an unsat core says what a proof used.
  std::vector<std::pair<std::string, std::string>> flags;
  std::vector<Decl> consts;
  std::vector<std::pair<std::string, T>> defs; // Bool definitions, in order
  std::vector<T> hypotheses;                   // asserted as they are
  T goal;                                      // asserted negated: unsat means valid

  std::vector<T> values;   // if sat, the values of these are wanted
  bool core = false;       // if unsat, the unsat core is wanted (definitions are then named)
  bool definitions = true; // false: use no definitions at all
  long long rlimit = 0;    // 0: the background's limit
};

// The function symbols a term applies.
void symbols(const T &t, std::set<std::string> &out);

// The definitions a query needs: those of the symbols it mentions, and of
// the symbols those definitions mention.  A query gets no others: an
// unneeded quantified definition can keep Z3 from finding a counterexample,
// so that it answers unknown instead of sat.
std::set<std::string> definitionsNeeded(const Background &bg, const Query &q);

// SMT-LIB text.  A whole query is its header followed by its body.
std::string render(const Axiom &a, bool named = false);
// Options, sorts and function declarations: what every query starts with.
std::string renderPrelude(const Background &bg, long long rlimit);
// The prelude and the definitions this query needs.
std::string renderHeader(const Background &bg, const Query &q);
// Declarations, the negated goal, check-sat, and what is asked afterwards.
std::string renderBody(const Query &q);
std::string render(const Background &bg, const Query &q);

} // namespace verutop

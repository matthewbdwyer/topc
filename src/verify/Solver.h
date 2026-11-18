#pragma once

#include "Query.h"

#include <functional>
#include <string>
#include <vector>

namespace verutop {

// ===========================================================================
// The solver is the verifier's back end: a narrow interface, one real
// implementation, and one for tests.
// ===========================================================================

enum class Verdict {
  Unsat,   // the goal is valid: proved
  Sat,     // there is a counterexample
  Unknown, // the solver gave up
  Error    // it said something else; see Answer::raw
};

struct Answer {
  Verdict verdict = Verdict::Error;
  std::string raw;      // the solver's first line, as it wrote it
  bool limited = false; // Unknown because the resource limit was reached
  std::string rest;     // whatever it printed after the answer
  // The values asked for, as text, in the order of Query::values; empty
  // if the solver gave none.  (- 1) is -1; (|Node| 1 |Leaf|) is Node(1, Leaf).
  std::vector<std::string> values;
  std::vector<std::string> core; // Unsat, and the core was asked for: its names
};

class Solver {
public:
  virtual ~Solver() = default;
  virtual Answer check(const Background &bg, const Query &q) = 0;
  // As named in the trust report.
  virtual std::string version() = 0;
};

// Z3 as a subprocess, one process per query, the query on its standard
// input.  (In one process, state left by an earlier query, even after
// push/pop, can make Z3 give up on a later one.)  The only thing shared
// with Z3 is SMT-LIB text: the same text --psmt shows, plus one line that
// asks, after check-sat, why an answer is `unknown`.
class Z3Process : public Solver {
public:
  Answer check(const Background &bg, const Query &q) override;
  std::string version() override;
};

// Answers from a function, for tests; remembers what it was asked.
class CannedSolver : public Solver {
public:
  using Rule = std::function<Answer(const Query &)>;
  explicit CannedSolver(Rule rule) : rule(std::move(rule)) {}
  Answer check(const Background &, const Query &q) override {
    asked.push_back(q);
    return rule(q);
  }
  std::string version() override { return "a canned solver"; }
  std::vector<Query> asked;

  static Answer unsat(std::vector<std::string> core = {});
  static Answer sat(std::vector<std::string> values = {});
  static Answer unknown(bool limited);

private:
  Rule rule;
};

// The text sent for a query: its rendering, with the question about
// `unknown` added after check-sat.
std::string z3Input(const Background &bg, const Query &q);
// Z3's output for that text, as an Answer.
Answer parseAnswer(const std::string &output, const Query &q);

} // namespace verutop

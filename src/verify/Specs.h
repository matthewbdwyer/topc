#pragma once

#include "Lowering.h"

namespace verutop {

// ===========================================================================
// Spec functions.  A spec function is pure: no loops, input, output, error,
// or calls to ordinary functions.  With each if and case merged into an ite,
// its body is one term; it becomes an uninterpreted function and a defining
// axiom, triggered on each application.  A recursive call, to itself or
// around a cycle of spec functions, must decrease the measure: a definition
// that does not terminate, like f(n) = f(n) + 1, can be false, and from it
// Z3 proves anything.
// ===========================================================================
class SpecFns {
public:
  std::string decls;                   // declare-fun for each
  std::map<std::string, Axiom> axioms; // each one's definition
  std::map<std::string, T> lhs, rhs;         // the definition, for --pspec
  struct Check {
    std::string fn;
    Obligation ob;
    Query query;
  };
  std::vector<Check> checks; // termination obligations
  std::vector<std::string> skipped;

  SpecFns(ASTProgram *p, Lower &low);

private:
  void build(ASTProgram *p);
  void stmt(ASTStmt *s);
  void termination(ASTFunction *g, const std::vector<T> &args, ASTNode *call);

  Lower &low;
  ASTFunction *current = nullptr;
  Sub env, params0;
  std::map<std::string, std::string> sorts;
  std::vector<T> path; // the conditions of the enclosing ifs and arms
  bool inMeasure = false;
};

} // namespace verutop

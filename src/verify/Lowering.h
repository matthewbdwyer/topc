#pragma once

#include "Encoding.h"
#include "Gcl.h"
#include "Obligations.h"
#include "Term.h"
#include "Termination.h"

#include "AST.h"

#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace verutop {

// ===========================================================================
// Lowering: AST to guarded commands.
// ===========================================================================

struct Proc {
  ASTFunction *fn = nullptr;
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

std::string where(ASTNode *n);
std::string text(ASTNode *n);
// The variable an lvalue or a borrowed argument names: x, *x, **x, &x.
std::string rootOf(ASTExpr *e, int *derefs = nullptr);

// Which borrowed parameters a function may write: through *p = E, or by
// passing p on to a parameter that is written.  (Only an int can be written
// through a borrow; TOP rejects overwriting an owned value.)
class Writes {
public:
  std::set<std::pair<std::string, int>> written; // (function, parameter index)

  Writes(ASTProgram *p, const Encoding &enc);
  bool isWritten(const std::string &f, int k) const { return written.count({f, k}); }
  // The variables a statement may change: assignment targets, and variables
  // lent to a parameter that is written.
  std::set<std::string> assigned(ASTNode *s) const;

private:
  std::map<std::string, ASTFunction *> fns;
};

// A case arm as a condition on the scrutinee t, and the binder values.
T matchPattern(const Encoding &enc, ASTPattern *p, const T &t,
               std::vector<std::pair<ASTDeclNode *, T>> &binds);
T matchCtor(const Encoding &enc, const std::string &tag, const std::vector<ASTPattern *> &subs,
            const T &t, std::vector<std::pair<ASTDeclNode *, T>> &binds);
std::string showArm(ASTCaseArm *a);

class Lower {
public:
  Lower(ASTProgram *p, const Encoding &enc, const Writes &w);

  const Encoding &enc;

  std::map<std::string, ASTFunction *> fns;
  // The first pass over the program records its calls here; the second
  // knows the cycles they form.
  CallEdges *edges = nullptr;
  const Cycles *cycles = nullptr;
  // --overflow: arithmetic in code must stay in 64 bits.
  bool overflow = false;
  // Called on each spec-function call in a specification (for termination).
  std::function<void(ASTFunction *, const std::vector<T> &, int)> onSpecCall;
  // The sort of a variable, for measures.
  std::function<std::string(const std::string &)> sortLookup;

  // A function as guarded commands, with its obligations.
  Proc proc(ASTFunction *f);

  // An expression as an int term, or as a condition.  `pre` collects the
  // commands the expression needs first; null means a specification.
  T I(ASTExpr *e, std::vector<G> *pre, const Sub *sub);
  T B(ASTExpr *e, std::vector<G> *pre, const Sub *sub);
  T spec(ASTExpr *e, const Sub *sub = nullptr, const Sub *olds = nullptr);
  // A measure is an int (the type checker sees to it); rank(t) makes one
  // from a sum value.
  T measure(ASTExpr *e, const Sub *sub = nullptr) { return I(e, nullptr, sub); }

private:
  const Writes &writes;
  ASTFunction *current = nullptr;
  Proc proc_;
  std::vector<Obligation> obs;
  Sub old, oldAll;
  const Sub *oldSub = nullptr;
  std::map<std::string, std::string> renames; // case binders that shadow
  int fresh = 0;
  int ghost = 0; // inside the arguments of `use`

  // Whether values here are 64-bit values that an obligation keeps in range.
  bool machineInts() const;
  std::string name(const std::string &x) const;
  G havoc(std::vector<std::string> xs, int line, std::string note);
  static G guard(T p, T cond, int line, std::string note);
  G tracked(G g, Assumption::Kind kind, std::string what, std::string callee = "");
  G missingClause(int line, std::string what);
  G assertion(T p, int line, std::string what);
  G stmt(ASTStmt *s);
  T callExpr(ASTFunAppExpr *call, std::vector<G> &cs, const std::string &target);
};

} // namespace verutop

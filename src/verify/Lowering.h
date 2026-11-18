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

std::string where(ASTNode *n);
std::string text(ASTNode *n);
std::string showArm(ASTCaseArm *a);

// Where an expression is being translated: what the translation needs to
// know that depends on it.
class Scope {
public:
  virtual ~Scope() = default;
  // The sort of a variable (for rank, and for the binders of a case).
  virtual std::string sortOf(const std::string &x) const = 0;
  // A specification calls a spec function.
  virtual void specCall(ASTFunction * /*callee*/, const std::vector<T> & /*args*/, ASTNode * /*at*/) {}
};

// AST to guarded commands, one function at a time.  The class is one
// translation in four files: Lowering.cpp (a function, and the commands
// every part builds), LowerExpr.cpp, LowerStmt.cpp, LowerCall.cpp.
class Lower {
public:
  Lower(ASTProgram *p, const Encoding &enc, bool overflow);

  const Encoding &enc;

  // The calls between functions (and lemmas), found by lowering each once.
  CallEdges calls();
  // From now on a call around a cycle of functions gets a termination
  // obligation, as a call from a function to itself always does.
  void recursion(const Cycles &c) { cycles = &c; }

  // A function as guarded commands, with its obligations and assumptions.
  Proc proc(ASTFunction *f);

  // ---- expressions (LowerExpr.cpp) ------------------------------------------
  // An expression as an int term (I), or as a condition (B): a TOP
  // condition is true when it is not zero.  `pre` collects the commands the
  // expression needs first: an assert for each division, a havoc for each
  // input, a call's contract.  A null `pre` means a specification, which
  // has none of these.  `sub` renames variables; inside old(E), the
  // substitution given to `spec` does.
  T I(ASTExpr *e, std::vector<G> *pre, const Sub *sub);
  T B(ASTExpr *e, std::vector<G> *pre, const Sub *sub);
  T spec(ASTExpr *e, const Sub *sub = nullptr, const Sub *olds = nullptr);
  // A measure is an int (the type checker sees to it); rank(t) makes one
  // from a sum value.
  T measure(ASTExpr *e, const Sub *sub = nullptr) { return I(e, nullptr, sub); }
  // The sort of a term, in the current scope.
  std::string sortOf(const T &t) const;

  // Translate in `scope` for as long as this object lives.  Specs uses it
  // to translate the bodies of spec functions; code is its own scope.
  class In {
  public:
    In(Lower &low, Scope &scope) : low(low), saved(low.scope) { low.scope = &scope; }
    ~In() { low.scope = saved; }

  private:
    Lower &low;
    Scope *saved;
  };

private:
  // The scope of code: a variable's sort is the one its function gives it.
  struct CodeScope : Scope {
    const Proc *proc = nullptr;
    std::string sortOf(const std::string &x) const override { return proc->sortOf(x); }
  } code;
  Scope *scope = &code;

  std::vector<ASTFunction *> order; // the program's functions
  std::map<std::string, ASTFunction *> fns;
  bool overflow;                    // --overflow: arithmetic in code must stay in 64 bits
  CallEdges *edges = nullptr;       // while finding the calls
  const Cycles *cycles = nullptr;

  ASTFunction *current = nullptr;
  Proc proc_;
  std::vector<Obligation> obs;
  Sub old, oldAll;
  const Sub *oldSub = nullptr;
  std::map<std::string, std::string> renames; // case binders that shadow
  int fresh = 0;
  int ghost = 0; // inside the arguments of `use`

  // ---- the commands every part builds (Lowering.cpp) --------------------------
  // Whether values here are 64-bit values that an obligation keeps in range.
  bool machineInts() const;
  std::string name(const std::string &x) const;
  // An unknown value; an int in code is still a 64-bit value.
  G havoc(std::vector<std::string> xs, int line, std::string note);
  // The assume that starts a branch; `cond` is the source condition.
  static Assume guard(T p, T cond, int line, std::string note);
  // An assume that a proof may be found to depend on.
  Assume tracked(Assume g, Assumption::Kind kind, std::string what, std::string callee = "");
  // A new obligation of `kind` at `at`, claiming p.  A missing clause (see
  // ObKind) claims false, fails, and is not assumed afterwards.
  Assert assertion(T p, ASTNode *at, ObKind kind, std::string subject = "", std::string callee = "");
  Assert assertion(T p, Obligation ob);

  // ---- statements (LowerStmt.cpp) and calls (LowerCall.cpp) ---------------------
  G stmt(ASTStmt *s);
  T callExpr(ASTFunAppExpr *call, std::vector<G> &cs, const std::string &target);
};

} // namespace verutop

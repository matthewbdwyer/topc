// VERUTOP prototype: verification conditions for a subset of TOP.
//
// The pipeline; each stage after the AST can be printed:
//
//   AST  --lower-->      guarded commands              --pgcl
//        --passivize-->  passive commands (versions)   --ppassive
//        --vc-->         one formula per function      --pvc, --pwp (outline)
//        -->             one SMT-LIB query per obligation  --psmt
//        --> Z3: proved, or a counterexample           --pcex (as a trace)
//
// Guarded commands have five forms: assume P, assert P, x := E, havoc x,
// and a choice between two commands, plus sequencing.  Every TOP construct
// the prototype supports lowers to these, so both VC generators (wp by
// substitution, and passive) work on guarded commands, not on the AST.
//
// Heap data needs no heap model, because of ownership:
// - An owned box has one owner, so a variable of type own&T is represented
//   by the value it points to: alloc E, *E, and &E are all the identity, and
//   *p = E assigns p.
// - A sum value is an SMT datatype value; case is a chain of constructor
//   tests, and its binders are selectors.
// - A borrow exists only for the duration of a call, and only an int can be
//   written through one.  So a borrowed parameter is an in-out parameter: the
//   callee's ensures says what *p is at the end (and old(*p) what it was), and
//   the caller's variable takes that value.  A borrowed parameter the callee
//   never writes leaves the caller's variable alone, with no frame condition.
//
// Supported: ints and sum types; + - * / and comparisons; if, while (with
// invariants and decreases), case, blocks, assignment, output, error, input;
// alloc, *, & (as a call argument); calls to functions by name, anywhere in
// an expression, verified modularly against the callee's contract; assert
// and assume; spec functions.  Function values are not supported.
#include "VCGen.h"

#include "AST.h"
#include "ASTCtorPattern.h"
#include "ASTOldExpr.h"
#include "ASTProofStmt.h"
#include "ASTRankExpr.h"
#include "ASTVarPattern.h"
#include "ASTVisitor.h"
#include "ASTWildcardPattern.h"
#include "ReferenceMode.h"
#include "ReferenceType.h"
#include "SemanticError.h"
#include "TopFunction.h"
#include "TopInt.h"
#include "TopMu.h"
#include "TopSumType.h"
#include "TypeInference.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct Unsupported : std::runtime_error {
  using std::runtime_error::runtime_error;
};

struct Obligation {
  int line;
  std::string what;
};

// ===========================================================================
// Terms.  One representation, printed two ways: SMT-LIB for Z3, and infix
// for people.
// ===========================================================================

struct Term;
using T = std::shared_ptr<const Term>;
struct Term {
  enum Kind { Num, Var, Op, Fn, Ctor, Is, Sel, Let, Forall } kind = Num;
  long long num = 0;
  std::string name;               // Var, Op, Fn, Ctor, Is (tag), Sel (selector);
                                  // Let: the bound variable
  std::vector<std::string> bound; // Forall
  std::vector<std::string> sorts; // Forall: the sort of each bound variable
  std::vector<T> args;            // Op, Fn, Ctor, Is, Sel; Let: value, body;
                                  // Forall: body
};

T make(Term t) { return std::make_shared<const Term>(std::move(t)); }
T num(long long n) {
  Term t;
  t.num = n;
  return make(t);
}
T var(const std::string &x) {
  Term t;
  t.kind = Term::Var;
  t.name = x;
  return make(t);
}
T mk(Term::Kind k, const std::string &n, std::vector<T> a) {
  Term t;
  t.kind = k;
  t.name = n;
  t.args = std::move(a);
  return make(t);
}
T op(const std::string &o, std::vector<T> a = {}) { return mk(Term::Op, o, std::move(a)); }
// An application of a spec function.
T app(const std::string &f, std::vector<T> a) { return mk(Term::Fn, f, std::move(a)); }
T ctor(const std::string &tag, std::vector<T> a) { return mk(Term::Ctor, tag, std::move(a)); }
T is(const std::string &tag, T x) { return mk(Term::Is, tag, {std::move(x)}); }
T sel(const std::string &s, T x) { return mk(Term::Sel, s, {std::move(x)}); }
T let(const std::string &x, T v, T body) { return mk(Term::Let, x, {std::move(v), std::move(body)}); }
T forall(std::vector<std::string> xs, std::vector<std::string> sorts, T body) {
  if (xs.empty()) return body;
  Term t;
  t.kind = Term::Forall;
  t.bound = std::move(xs);
  t.sorts = std::move(sorts);
  t.args = {std::move(body)};
  return make(t);
}
T rebuild(const T &t, std::vector<T> a) {
  Term c = *t;
  c.args = std::move(a);
  return make(c);
}

const T TRUE = op("true"), FALSE = op("false");
bool isOp(const T &t, const char *o) { return t->kind == Term::Op && t->name == o; }
bool isTrue(const T &t) { return isOp(t, "true"); }
bool isFalse(const T &t) { return isOp(t, "false"); }

T And(const std::vector<T> &all) {
  std::vector<T> ps;
  for (auto &p : all)
    if (!isTrue(p)) ps.push_back(p);
  if (ps.empty()) return TRUE;
  if (ps.size() == 1) return ps[0];
  return op("and", ps);
}
T Not(T p) { return op("not", {std::move(p)}); }
T Implies(T p, T q) { return op("=>", {std::move(p), std::move(q)}); }
T Eq(T a, T b) { return op("=", {std::move(a), std::move(b)}); }

// ---- SMT-LIB --------------------------------------------------------------

void smt(const T &t, std::string &o) {
  auto apply = [&](const std::string &head) {
    if (t->args.empty()) {
      o += head;
      return;
    }
    o += "(" + head;
    for (auto &a : t->args) {
      o += " ";
      smt(a, o);
    }
    o += ")";
  };
  switch (t->kind) {
  case Term::Num:
    o += t->num < 0 ? "(- " + std::to_string(-t->num) + ")" : std::to_string(t->num);
    return;
  case Term::Var: o += "|" + t->name + "|"; return;
  case Term::Op: apply(t->name); return;
  case Term::Fn:
  case Term::Ctor:
  case Term::Sel: apply("|" + t->name + "|"); return;
  case Term::Is: apply("(_ is |" + t->name + "|)"); return;
  case Term::Let:
    o += "(let ((|" + t->name + "| ";
    smt(t->args[0], o);
    o += ")) ";
    smt(t->args[1], o);
    o += ")";
    return;
  case Term::Forall:
    o += "(forall (";
    for (size_t i = 0; i < t->bound.size(); ++i)
      o += "(|" + t->bound[i] + "| " + (i < t->sorts.size() ? t->sorts[i] : "Int") + ")";
    o += ") ";
    smt(t->args[0], o);
    o += ")";
    return;
  }
}
std::string smt(const T &t) {
  std::string o;
  smt(t, o);
  return o;
}

// ---- infix ----------------------------------------------------------------

bool isCmp(const std::string &o) {
  return o == "=" || o == "<" || o == "<=" || o == ">" || o == ">=";
}
int prec(const T &t) {
  if (t->kind == Term::Let || t->kind == Term::Forall) return 0;
  if (t->kind == Term::Is) return 5;
  if (t->kind != Term::Op || t->args.empty()) return 9;
  auto &o = t->name;
  if (o == "=>") return 1;
  if (o == "or") return 2;
  if (o == "and") return 3;
  if (o == "not") return isOp(t->args[0], "=") ? 5 : 8;
  if (isCmp(o)) return 5;
  if (o == "+" || o == "-") return 6;
  if (o == "*" || o == "tdiv") return 7;
  return 9;
}
std::string sym(const std::string &o) {
  if (o == "=") return "==";
  if (o == "and") return "&&";
  if (o == "or") return "||";
  if (o == "=>") return "==>";
  if (o == "tdiv") return "/";
  return o;
}

std::string flat(const T &t);
std::string paren(const T &c, bool need) {
  auto s = flat(c);
  return need ? "(" + s + ")" : s;
}
std::string args(const T &t) {
  std::string s = "(";
  for (size_t i = 0; i < t->args.size(); ++i) s += (i ? ", " : "") + flat(t->args[i]);
  return s + ")";
}
std::string flat(const T &t) {
  switch (t->kind) {
  case Term::Num: return std::to_string(t->num);
  case Term::Var: return t->name;
  case Term::Fn: return (t->name.rfind("rank#", 0) == 0 ? "rank" : t->name) + args(t);
  case Term::Ctor: return t->args.empty() ? t->name : t->name + args(t);
  case Term::Is: return paren(t->args[0], prec(t->args[0]) <= 5) + " is " + t->name;
  case Term::Sel:
    return paren(t->args[0], prec(t->args[0]) < 9) + "." + t->name.substr(t->name.find('.') + 1);
  case Term::Let:
    return "let " + t->name + " = " + flat(t->args[0]) + " in " + flat(t->args[1]);
  case Term::Forall: {
    std::string s = "forall ";
    for (size_t i = 0; i < t->bound.size(); ++i) s += (i ? ", " : "") + t->bound[i];
    return s + " :: " + flat(t->args[0]);
  }
  case Term::Op: break;
  }
  auto &o = t->name;
  auto &a = t->args;
  if (a.empty()) return o;
  if (o == "not") {
    if (isOp(a[0], "=")) {
      auto &e = a[0]->args;
      return paren(e[0], prec(e[0]) <= 5) + " != " + paren(e[1], prec(e[1]) <= 5);
    }
    return "!" + paren(a[0], prec(a[0]) < 8);
  }
  if (o == "ite") return "(" + flat(a[0]) + " ? " + flat(a[1]) + " : " + flat(a[2]) + ")";
  int p = prec(t);
  if (o == "and" || o == "or") {
    std::string s;
    for (size_t i = 0; i < a.size(); ++i)
      s += (i ? " " + sym(o) + " " : "") + paren(a[i], prec(a[i]) <= p);
    return s;
  }
  if (o == "=>") return paren(a[0], prec(a[0]) <= 1) + " ==> " + paren(a[1], prec(a[1]) < 1);
  bool assoc = o == "+" || o == "*";
  return paren(a[0], isCmp(o) ? prec(a[0]) <= p : prec(a[0]) < p) + " " + sym(o) + " " +
         paren(a[1], assoc ? prec(a[1]) < p : prec(a[1]) <= p);
}

// Infix over several lines: a formula wider than `width` is broken at its
// top-level connectives.
std::vector<std::string> pretty(const T &t, int width) {
  if (width < 24) width = 24;
  auto s = flat(t);
  if ((int)s.size() <= width) return {s};
  auto child = [&](const T &c, bool need, int w) {
    auto L = pretty(c, need ? w - 2 : w);
    if (need) {
      L.front() = "(" + L.front();
      for (size_t i = 1; i < L.size(); ++i) L[i] = " " + L[i];
      L.back() += ")";
    }
    return L;
  };
  std::vector<std::string> out;
  if (isOp(t, "and") || isOp(t, "or")) {
    for (size_t i = 0; i < t->args.size(); ++i) {
      auto L = child(t->args[i], prec(t->args[i]) <= prec(t), width);
      if (i + 1 < t->args.size()) L.back() += " " + sym(t->name);
      out.insert(out.end(), L.begin(), L.end());
    }
    return out;
  }
  if (isOp(t, "=>")) {
    out = child(t->args[0], prec(t->args[0]) <= 1, width);
    out.back() += " ==>";
    for (auto &l : child(t->args[1], prec(t->args[1]) < 1, width - 2)) out.push_back("  " + l);
    return out;
  }
  if (t->kind == Term::Let) {
    out.push_back("let " + t->name + " = " + flat(t->args[0]) + " in");
    for (auto &l : pretty(t->args[1], width)) out.push_back(l);
    return out;
  }
  if (t->kind == Term::Forall) {
    std::string h = "forall ";
    for (size_t i = 0; i < t->bound.size(); ++i) h += (i ? ", " : "") + t->bound[i];
    out.push_back(h + " ::");
    for (auto &l : pretty(t->args[0], width - 2)) out.push_back("  " + l);
    return out;
  }
  return {s};
}

// ---- substitution and simplification ---------------------------------------

using Sub = std::map<std::string, T>;

T subst(const T &t, const Sub &m) {
  if (m.empty()) return t;
  switch (t->kind) {
  case Term::Num: return t;
  case Term::Var: {
    auto it = m.find(t->name);
    return it == m.end() ? t : it->second;
  }
  case Term::Let: {
    auto inner = m;
    inner.erase(t->name);
    return let(t->name, subst(t->args[0], m), subst(t->args[1], inner));
  }
  case Term::Forall: {
    auto inner = m;
    for (auto &x : t->bound) inner.erase(x);
    return forall(t->bound, t->sorts, subst(t->args[0], inner));
  }
  default: break;
  }
  std::vector<T> a;
  bool changed = false;
  for (auto &x : t->args) {
    a.push_back(subst(x, m));
    changed = changed || a.back() != x;
  }
  return changed ? rebuild(t, a) : t;
}

void freeVars(const T &t, std::set<std::string> &out, const std::set<std::string> &bound = {}) {
  switch (t->kind) {
  case Term::Num: return;
  case Term::Var:
    if (!bound.count(t->name)) out.insert(t->name);
    return;
  case Term::Let: {
    freeVars(t->args[0], out, bound);
    auto b = bound;
    b.insert(t->name);
    freeVars(t->args[1], out, b);
    return;
  }
  case Term::Forall: {
    auto b = bound;
    b.insert(t->bound.begin(), t->bound.end());
    freeVars(t->args[0], out, b);
    return;
  }
  default:
    for (auto &a : t->args) freeVars(a, out, bound);
  }
}

// For people only: fold constants and drop trivial connectives.  What Z3
// sees is never simplified.
T simp(const T &t) {
  if (t->kind == Term::Num || t->kind == Term::Var) return t;
  if (t->kind == Term::Forall) {
    auto b = simp(t->args[0]);
    std::set<std::string> fv;
    freeVars(b, fv);
    std::vector<std::string> keep, sorts;
    for (size_t i = 0; i < t->bound.size(); ++i)
      if (fv.count(t->bound[i])) {
        keep.push_back(t->bound[i]);
        sorts.push_back(i < t->sorts.size() ? t->sorts[i] : "Int");
      }
    return forall(keep, sorts, b);
  }
  if (t->kind == Term::Let) return simp(subst(t->args[1], {{t->name, t->args[0]}}));
  std::vector<T> a;
  for (auto &x : t->args) a.push_back(simp(x));
  if (t->kind == Term::Is && a[0]->kind == Term::Ctor) return a[0]->name == t->name ? TRUE : FALSE;
  if (t->kind == Term::Sel && a[0]->kind == Term::Ctor) {
    // Node.left of Node(v, l, r) is l: the field's position in the selector
    // is not recorded here, so this only folds when the name says which.
    return rebuild(t, a);
  }
  if (t->kind != Term::Op) return rebuild(t, a);
  auto &o = t->name;
  auto isNum = [](const T &x) { return x->kind == Term::Num; };
  auto isNumV = [&](const T &x, long long v) { return isNum(x) && x->num == v; };
  if (a.size() == 2 && isNum(a[0]) && isNum(a[1])) {
    long long l = a[0]->num, r = a[1]->num;
    if (o == "+") return num(l + r);
    if (o == "-") return num(l - r);
    if (o == "*") return num(l * r);
    if (o == "tdiv" && r != 0) return num(l / r);
    if (o == "<") return l < r ? TRUE : FALSE;
    if (o == "<=") return l <= r ? TRUE : FALSE;
    if (o == ">") return l > r ? TRUE : FALSE;
    if (o == ">=") return l >= r ? TRUE : FALSE;
    if (o == "=") return l == r ? TRUE : FALSE;
  }
  if (o == "+" && isNumV(a[1], 0)) return a[0];
  if (o == "+" && isNumV(a[0], 0)) return a[1];
  if (o == "-" && isNumV(a[1], 0)) return a[0];
  if (o == "*" && isNumV(a[1], 1)) return a[0];
  if (o == "*" && isNumV(a[0], 1)) return a[1];
  if (o == "*" && (isNumV(a[0], 0) || isNumV(a[1], 0))) return num(0);
  if ((o == "=" || o == "<=" || o == ">=") && smt(a[0]) == smt(a[1])) return TRUE;
  if (o == "=" && a[0]->kind == Term::Ctor && a[1]->kind == Term::Ctor &&
      a[0]->name != a[1]->name)
    return FALSE;
  if (o == "not") {
    if (isTrue(a[0])) return FALSE;
    if (isFalse(a[0])) return TRUE;
    if (isOp(a[0], "not")) return a[0]->args[0];
    return Not(a[0]);
  }
  if (o == "and" || o == "or") {
    bool isAnd = o == "and";
    std::vector<T> ps;
    std::set<std::string> seen;
    std::function<void(const T &)> add = [&](const T &p) {
      if (isOp(p, o.c_str())) {
        for (auto &q : p->args) add(q);
        return;
      }
      if (isAnd ? isTrue(p) : isFalse(p)) return;
      if (seen.insert(smt(p)).second) ps.push_back(p);
    };
    for (auto &p : a) add(p);
    for (auto &p : ps)
      if (isAnd ? isFalse(p) : isTrue(p)) return p;
    if (ps.empty()) return isAnd ? TRUE : FALSE;
    if (ps.size() == 1) return ps[0];
    return op(o, ps);
  }
  if (o == "=>") {
    if (isTrue(a[0])) return a[1];
    if (isFalse(a[0]) || isTrue(a[1])) return TRUE;
    if (smt(a[0]) == smt(a[1])) return TRUE;
    if (isFalse(a[1])) return simp(Not(a[0]));
  }
  if (o == "ite") {
    if (isTrue(a[0])) return a[1];
    if (isFalse(a[0])) return a[2];
  }
  return op(o, a);
}

// ===========================================================================
// Sorts.  int is Int; a sum type is an SMT datatype; a reference (owned or
// borrowed) has the sort of what it refers to.
// ===========================================================================

struct Types {
  TypeInference *ti = nullptr;
  std::map<std::string, std::string> ctorSort;                             // tag -> sort
  std::map<std::string, std::vector<std::pair<std::string, std::string>>> fields; // tag -> (selector, sort)
  std::map<std::string, std::string> selSort;                              // selector -> sort
  std::map<std::string, std::string> fnSort;                               // function -> result sort
  std::map<std::string, std::string> rankAxioms;                           // rank#D -> axiom
  std::string decls;                                                       // datatypes and rank functions

  static bool isDatatype(const std::string &s) { return s != "Int" && s != "Bool"; }

  std::string sortOf(const std::shared_ptr<TopType> &t) const {
    if (!t || dynamic_cast<TopInt *>(t.get())) return "Int";
    if (auto *r = dynamic_cast<ReferenceType *>(t.get())) return sortOf(r->getReferencedType());
    if (auto *s = dynamic_cast<TopSumType *>(t.get())) return "|" + s->getTypeName() + "|";
    if (auto *m = dynamic_cast<TopMu *>(t.get())) return sortOf(m->getT());
    if (dynamic_cast<TopFunction *>(t.get())) throw Unsupported("function values are not supported");
    return "Int"; // an unconstrained type variable
  }
  static bool isBorrow(const std::shared_ptr<TopType> &t) {
    auto *r = dynamic_cast<ReferenceType *>(t.get());
    if (!r) return false;
    auto m = std::dynamic_pointer_cast<ReferenceMode>(r->getMode());
    return m && m->getMode() == ReferenceMode::Mode::Borrow;
  }
  std::string sortOf(ASTDeclNode *d) const { return sortOf(ti->getInferredType(d)); }
  bool isBorrow(ASTDeclNode *d) const { return isBorrow(ti->getInferredType(d)); }

  void build(ASTProgram *p, TypeInference *types) {
    ti = types;
    std::string dts, ctors;
    for (auto *td : p->getTypedecls()) {
      std::string s = "|" + td->getName() + "|";
      dts += " (" + s + " 0)";
      ctors += " (";
      for (auto *v : td->getVariants()) {
        ctorSort[v->getTag()] = s;
        ctors += "(|" + v->getTag() + "|";
        for (auto *f : v->getParams()) {
          std::string sl = v->getTag() + "." + f->getName(), so = sortOf(f);
          fields[v->getTag()].push_back({sl, so});
          selSort[sl] = so;
          ctors += " (|" + sl + "| " + so + ")";
        }
        ctors += ")";
      }
      ctors += ")";
    }
    if (!dts.empty()) decls += "(declare-datatypes (" + dts + ") (" + ctors + "))\n";
    for (auto *f : p->getFunctions()) {
      auto ft = std::dynamic_pointer_cast<TopFunction>(ti->getInferredType(f->getDecl()));
      try {
        fnSort[f->getName()] = ft ? sortOf(ft->getReturnType()) : "Int";
      } catch (Unsupported &) {
        fnSort[f->getName()] = "Int";
      }
    }
    // rank#D measures the size of a D value, for `decreases` on a datatype:
    // rank(C(x1, ..., xn)) = 1 + the ranks of the fields that are datatypes.
    // That a rank is at least 1 is true but needs induction, so it is
    // stated as part of the (trusted) axiom.
    for (auto *td : p->getTypedecls()) {
      std::string s = "|" + td->getName() + "|", r = "rank#" + td->getName();
      decls += "(declare-fun |" + r + "| (" + s + ") Int)\n";
      T x = var("x");
      T body = num(1);
      auto vs = td->getVariants();
      for (int i = (int)vs.size() - 1; i >= 0; --i) {
        std::vector<T> sum{num(1)};
        for (auto &[sl, so] : fields[vs[i]->getTag()])
          if (isDatatype(so)) sum.push_back(rank(sel(sl, x), so));
        T e = sum[0];
        for (size_t k = 1; k < sum.size(); ++k) e = op("+", {e, sum[k]});
        body = i + 1 == (int)vs.size() ? e : op("ite", {is(vs[i]->getTag(), x), e, body});
      }
      T call = app(r, {x});
      rankAxioms[r] = "(assert (forall ((|x| " + s + ")) (! (and (>= " + smt(call) + " 1) " +
                      smt(Eq(call, body)) + ") :pattern (" + smt(call) + "))))\n";
    }
  }

  static T rank(T t, const std::string &sort) {
    return app("rank#" + sort.substr(1, sort.size() - 2), {std::move(t)});
  }
};
Types TY;

// The sort of a term; `lookup` gives the sort of a variable.
std::string sortOfTerm(const T &t, const std::function<std::string(const std::string &)> &lookup) {
  switch (t->kind) {
  case Term::Num: return "Int";
  case Term::Var: return lookup(t->name);
  case Term::Fn: {
    auto it = TY.fnSort.find(t->name);
    return it == TY.fnSort.end() ? "Int" : it->second;
  }
  case Term::Ctor: return TY.ctorSort.count(t->name) ? TY.ctorSort[t->name] : "Int";
  case Term::Sel: return TY.selSort.count(t->name) ? TY.selSort[t->name] : "Int";
  case Term::Is:
  case Term::Forall: return "Bool";
  case Term::Let: return sortOfTerm(t->args[1], lookup);
  case Term::Op:
    if (t->name == "ite") return sortOfTerm(t->args[1], lookup);
    if (t->name == "+" || t->name == "-" || t->name == "*" || t->name == "tdiv") return "Int";
    return "Bool";
  }
  return "Int";
}

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
  bool guard = false;             // Assume: the condition that picks a branch
  T src; // a guard: the source condition; a passive assignment: the value
};

G gAssume(T p, int line, std::string note, bool guard = false) {
  G g;
  g.kind = G::Assume;
  g.p = std::move(p);
  g.line = line;
  g.note = std::move(note);
  g.guard = guard;
  return g;
}
G gAssign(std::string x, T e, int line, std::string note = "") {
  G g;
  g.kind = G::Assign;
  g.x = std::move(x);
  g.p = std::move(e);
  g.line = line;
  g.note = std::move(note);
  return g;
}
G gSeq(const std::vector<G> &ks) {
  G g;
  for (auto &k : ks) {
    if (k.kind == G::Seq)
      g.kids.insert(g.kids.end(), k.kids.begin(), k.kids.end());
    else
      g.kids.push_back(k);
  }
  return g;
}
G gChoice(G a, G b, int line) {
  G g;
  g.kind = G::Choice;
  g.kids = {std::move(a), std::move(b)};
  g.line = line;
  return g;
}

std::string pad(int n) { return std::string(n > 0 ? n : 0, ' '); }
std::string join(const std::vector<std::string> &xs, const std::string &sep = ", ") {
  std::string s;
  for (size_t i = 0; i < xs.size(); ++i) s += (i ? sep : "") + xs[i];
  return s;
}
std::string withComment(std::string s, const std::string &c, int ind = 0) {
  if (c.empty()) return s;
  size_t col = 44 - std::min(ind, 20);
  if (s.size() < col) s.resize(col, ' ');
  else s += "  ";
  return s + "// " + c;
}

// One primitive command as text lines (a formula may take several).
std::vector<std::string> showCmd(const G &c, bool passive, int ind = 0) {
  std::vector<std::string> out;
  auto formula = [&](const std::string &kw, const T &p, const std::string &comment) {
    auto L = pretty(p, 70 - (int)kw.size());
    for (size_t i = 0; i < L.size(); ++i) {
      std::string s = (i ? pad(kw.size()) : kw) + L[i];
      if (i + 1 == L.size()) s = withComment(s + ";", comment, ind);
      out.push_back(s);
    }
  };
  switch (c.kind) {
  case G::Assume: formula("assume ", c.p, c.note); break;
  case G::Assert:
    formula("assert ", c.p, "#" + std::to_string(c.id) + " line " + std::to_string(c.line) +
                                ": " + c.note);
    break;
  case G::Assign: formula(c.x + " := ", c.p, c.note); break;
  case G::Havoc:
    if (passive)
      out.push_back("// " + c.note + ": " + join(c.vars) + " are new versions");
    else
      out.push_back(withComment("havoc " + join(c.vars) + ";", c.note, ind));
    break;
  default: break;
  }
  return out;
}

void printG(const G &c, int ind, std::ostream &o, bool passive) {
  if (c.kind == G::Seq) {
    for (auto &k : c.kids) printG(k, ind, o, passive);
    return;
  }
  if (c.kind == G::Choice) {
    o << pad(ind) << "{\n";
    printG(c.kids[0], ind + 2, o, passive);
    o << pad(ind) << "} [] {\n";
    printG(c.kids[1], ind + 2, o, passive);
    o << pad(ind) << "}\n";
    return;
  }
  for (auto &l : showCmd(c, passive, ind - 2)) o << pad(ind) << l << "\n";
}

// ===========================================================================
// Lowering: AST to guarded commands.
// ===========================================================================

struct Proc {
  ASTFunction *fn = nullptr;
  G body;
  std::vector<Obligation> obs;
  std::map<std::string, std::string> sorts; // every variable's sort
  std::string sortOf(const std::string &x) const {
    auto it = sorts.find(x);
    return it == sorts.end() ? "Int" : it->second;
  }
};

std::string where(ASTNode *n) { return "line " + std::to_string(n->getLine()) + ": "; }
std::string text(ASTNode *n) {
  std::ostringstream o;
  o << *n;
  return o.str();
}

// The variable an lvalue or a borrowed argument names: x, *x, **x, &x.
std::string rootOf(ASTExpr *e, int *derefs = nullptr) {
  int d = 0;
  while (true) {
    if (auto *r = dynamic_cast<ASTDeRefExpr *>(e)) {
      e = r->getPtr();
      ++d;
    } else if (auto *b = dynamic_cast<ASTBorrowExpr *>(e)) {
      e = b->getVar();
    } else {
      break;
    }
  }
  if (derefs) *derefs = d;
  auto *v = dynamic_cast<ASTVariableExpr *>(e);
  return v ? v->getName() : "";
}

// Which borrowed parameters a function may write: through *p = E, or by
// passing p on to a parameter that is written.  (Only an int can be written
// through a borrow; TOP rejects overwriting an owned value.)
class Writes {
public:
  std::set<std::pair<std::string, int>> written; // (function, parameter index)

  explicit Writes(ASTProgram *p) {
    for (auto *f : p->getFunctions()) fns[f->getName()] = f;
    struct Scan : ASTVisitor {
      std::set<std::string> stores;                              // roots of *... = E
      std::vector<std::tuple<std::string, int, std::string>> passes; // (callee, k, root)
      void endVisit(ASTAssignStmt *a) override {
        int d = 0;
        auto r = rootOf(a->getLHS(), &d);
        if (d > 0) stores.insert(r);
      }
      void endVisit(ASTFunAppExpr *c) override {
        auto *fv = dynamic_cast<ASTVariableExpr *>(c->getFunction());
        if (!fv) return;
        auto as = c->getActuals();
        for (size_t k = 0; k < as.size(); ++k) passes.push_back({fv->getName(), (int)k, rootOf(as[k])});
      }
    };
    std::map<std::string, Scan> scans;
    for (auto *f : p->getFunctions()) f->accept(&scans[f->getName()]);
    bool changed = true;
    while (changed) {
      changed = false;
      for (auto *f : p->getFunctions()) {
        auto formals = f->getFormals();
        auto &sc = scans[f->getName()];
        for (size_t k = 0; k < formals.size(); ++k) {
          if (!TY.isBorrow(formals[k]) || written.count({f->getName(), (int)k})) continue;
          auto x = formals[k]->getName();
          bool w = sc.stores.count(x);
          for (auto &[g, j, r] : sc.passes)
            if (r == x && written.count({g, j})) w = true;
          if (w) {
            written.insert({f->getName(), (int)k});
            changed = true;
          }
        }
      }
    }
  }
  bool isWritten(const std::string &f, int k) const { return written.count({f, k}); }

  // The variables a statement may change: assignment targets, and variables
  // lent to a parameter that is written.
  std::set<std::string> assigned(ASTNode *s) const {
    struct Scan : ASTVisitor {
      const Writes *w;
      std::set<std::string> out;
      void endVisit(ASTAssignStmt *a) override { out.insert(rootOf(a->getLHS())); }
      void endVisit(ASTFunAppExpr *c) override {
        auto *fv = dynamic_cast<ASTVariableExpr *>(c->getFunction());
        if (!fv) return;
        auto as = c->getActuals();
        for (size_t k = 0; k < as.size(); ++k)
          if (w->isWritten(fv->getName(), (int)k)) out.insert(rootOf(as[k]));
      }
    } scan;
    scan.w = this;
    s->accept(&scan);
    scan.out.erase("");
    return scan.out;
  }

private:
  std::map<std::string, ASTFunction *> fns;
};

// A case arm as a condition on the scrutinee t, and the binder values.
T matchPattern(ASTPattern *p, const T &t, std::vector<std::pair<ASTDeclNode *, T>> &binds);
T matchCtor(const std::string &tag, const std::vector<ASTPattern *> &subs, const T &t,
            std::vector<std::pair<ASTDeclNode *, T>> &binds) {
  std::vector<T> cs{is(tag, t)};
  auto &fs = TY.fields[tag];
  for (size_t i = 0; i < subs.size() && i < fs.size(); ++i)
    cs.push_back(matchPattern(subs[i], sel(fs[i].first, t), binds));
  return And(cs);
}
T matchPattern(ASTPattern *p, const T &t, std::vector<std::pair<ASTDeclNode *, T>> &binds) {
  if (auto *c = dynamic_cast<ASTCtorPattern *>(p)) return matchCtor(c->getTag(), c->getSubPatterns(), t, binds);
  if (auto *v = dynamic_cast<ASTVarPattern *>(p)) binds.push_back({v->getDecl(), t});
  return TRUE;
}
std::string showArm(ASTCaseArm *a) {
  std::string s = a->getTag();
  auto ps = a->getPatterns();
  if (ps.empty()) return s;
  s += "(";
  for (size_t i = 0; i < ps.size(); ++i) {
    std::ostringstream o;
    o << *ps[i];
    s += (i ? ", " : "") + o.str();
  }
  return s + ")";
}

class Lower {
public:
  Lower(ASTProgram *p, const Writes &w) : writes(w) {
    for (auto *f : p->getFunctions()) fns[f->getName()] = f;
  }

  std::map<std::string, ASTFunction *> fns;
  // Called on each spec-function call in a specification (for termination).
  std::function<void(ASTFunction *, const std::vector<T> &, int)> onSpecCall;
  // The sort of a variable, for measures.
  std::function<std::string(const std::string &)> sortLookup;

  // A function: assume the precondition; locals start at 0; the body;
  // result := the returned value; assert the postcondition.
  //
  // In ensures, a value formal the body assigns means its entry value,
  // old(x): the caller cannot see the change.  A borrowed formal is shared
  // with the caller, so *p means its final contents and old(*p) its entry
  // contents.
  Proc proc(ASTFunction *f) {
    obs.clear();
    fresh = 0;
    current = f;
    renames.clear();
    proc_ = Proc{};
    proc_.fn = f;
    auto &sorts = proc_.sorts;
    sortLookup = [this](const std::string &x) { return proc_.sortOf(x); };
    for (auto *p : f->getFormals()) sorts[p->getName()] = TY.sortOf(p);
    for (auto *d : f->getDeclarations())
      for (auto *v : d->getVars()) sorts[v->getName()] = TY.sortOf(v);
    sorts["result"] = TY.fnSort[f->getName()];

    std::set<std::string> assigned;
    for (auto *s : f->getStmts()) {
      auto a = writes.assigned(s);
      assigned.insert(a.begin(), a.end());
    }
    old.clear();
    oldAll.clear();
    for (auto *p : f->getFormals()) {
      auto x = p->getName();
      sorts["old(" + x + ")"] = sorts[x];
      if (!assigned.count(x)) continue;
      oldAll[x] = var("old(" + x + ")");
      if (!TY.isBorrow(p)) old[x] = oldAll[x];
    }

    std::vector<G> cs;
    std::vector<T> req;
    for (auto *e : f->getRequires()) req.push_back(spec(e));
    if (!req.empty()) cs.push_back(gAssume(And(req), f->getRequires()[0]->getLine(), "requires"));
    for (auto *d : f->getDeclarations())
      for (auto *v : d->getVars()) {
        if (sorts[v->getName()] == "Int")
          cs.push_back(gAssign(v->getName(), num(0), d->getLine(), "locals start at 0"));
        else
          cs.push_back(havoc({v->getName()}, d->getLine(), "no initial value"));
      }
    auto stmts = f->getStmts();
    for (size_t i = 0; i + 1 < stmts.size(); ++i) cs.push_back(stmt(stmts[i]));
    auto *ret = dynamic_cast<ASTReturnStmt *>(stmts.back());
    std::vector<G> pre;
    T r = I(ret->getArg(), &pre, nullptr);
    cs.insert(cs.end(), pre.begin(), pre.end());
    cs.push_back(gAssign("result", r, ret->getLine(), "return"));
    std::vector<T> ens;
    for (auto *e : f->getEnsures()) ens.push_back(spec(e, &old, &oldAll));
    cs.push_back(assertion(And(ens), ret->getLine(), "postcondition"));
    proc_.body = gSeq(cs);
    proc_.obs = obs;
    return proc_;
  }

  // ---- expressions --------------------------------------------------------
  // `pre` collects the commands an expression needs first: an assert for
  // each division, a havoc for each input, a call's contract.  A null `pre`
  // means a specification, which has none of these.  `sub` renames
  // variables; inside old(E), `oldSub` does.

  T I(ASTExpr *e, std::vector<G> *pre, const Sub *sub) {
    if (auto *n = dynamic_cast<ASTNumberExpr *>(e)) return num(n->getValue());
    if (auto *v = dynamic_cast<ASTVariableExpr *>(e)) {
      if (sub) {
        auto it = sub->find(v->getName());
        if (it != sub->end()) return it->second;
      }
      return var(name(v->getName()));
    }
    // Ownership makes a reference its referent: one owner, no aliases.
    if (auto *a = dynamic_cast<ASTAllocExpr *>(e)) return I(a->getInitializer(), pre, sub);
    if (auto *d = dynamic_cast<ASTDeRefExpr *>(e)) return I(d->getPtr(), pre, sub);
    if (auto *b = dynamic_cast<ASTBorrowExpr *>(e)) return I(b->getVar(), pre, sub);
    if (auto *c = dynamic_cast<ASTSumCtorExpr *>(e)) {
      std::vector<T> as;
      for (auto *a : c->getArgs()) as.push_back(I(a, pre, sub));
      return ctor(c->getTag(), as);
    }
    if (auto *rk = dynamic_cast<ASTRankExpr *>(e)) {
      T t = I(rk->getExpr(), pre, sub);
      auto s = sortOfTerm(t, sortLookup);
      if (!Types::isDatatype(s)) throw Unsupported(where(e) + "rank applies only to a sum value");
      return Types::rank(t, s);
    }
    if (auto *o = dynamic_cast<ASTOldExpr *>(e)) {
      if (!oldSub) throw Unsupported(where(e) + "old(...) is allowed only in ensures");
      return I(o->getExpr(), pre, oldSub);
    }
    if (dynamic_cast<ASTInputExpr *>(e)) {
      if (!pre) throw Unsupported(where(e) + "input in a specification");
      std::string v = "input#" + std::to_string(fresh++);
      pre->push_back(havoc({v}, e->getLine(), "input"));
      return var(v);
    }
    if (auto *b = dynamic_cast<ASTBinaryExpr *>(e)) {
      using K = ASTBinaryExpr::BinaryOp;
      auto two = [&](const char *o) {
        auto l = I(b->getLeft(), pre, sub);
        auto r = I(b->getRight(), pre, sub);
        return op(o, {l, r});
      };
      switch (b->getOpKind()) {
      case K::Add: return two("+");
      case K::Sub: return two("-");
      case K::Mul: return two("*");
      case K::Div: {
        auto t = two("tdiv");
        if (pre)
          pre->push_back(assertion(Not(Eq(t->args[1], num(0))), e->getLine(),
                                   "divisor is not zero"));
        return t;
      }
      default: return op("ite", {B(e, pre, sub), num(1), num(0)});
      }
    }
    if (auto *call = dynamic_cast<ASTFunAppExpr *>(e)) {
      auto *fv = dynamic_cast<ASTVariableExpr *>(call->getFunction());
      if (!fv || !fns.count(fv->getName()))
        throw Unsupported(where(call) + "a call through a function value");
      auto *g = fns[fv->getName()];
      if (pre && g->isLemma())
        throw Unsupported(where(call) + "a lemma is applied only with use");
      if (pre) return callExpr(call, *pre, "");
      if (!g->isSpec())
        throw Unsupported(where(call) + "a specification may call only spec functions");
      std::vector<T> as;
      for (auto *a : call->getActuals()) as.push_back(I(a, nullptr, sub));
      if (onSpecCall) onSpecCall(g, as, call->getLine());
      return app(g->getName(), as);
    }
    throw Unsupported(where(e) + "unsupported expression in verification: " + text(e));
  }

  // A TOP condition is true when it is not zero.
  T B(ASTExpr *e, std::vector<G> *pre, const Sub *sub) {
    if (auto *b = dynamic_cast<ASTBinaryExpr *>(e)) {
      using K = ASTBinaryExpr::BinaryOp;
      auto ints = [&](const char *o) {
        auto l = I(b->getLeft(), pre, sub);
        auto r = I(b->getRight(), pre, sub);
        return op(o, {l, r});
      };
      auto bools = [&](const char *o) {
        auto l = B(b->getLeft(), pre, sub);
        auto r = B(b->getRight(), pre, sub);
        return op(o, {l, r});
      };
      switch (b->getOpKind()) {
      case K::Gt: return ints(">");
      case K::Lt: return ints("<");
      case K::Ge: return ints(">=");
      case K::Le: return ints("<=");
      case K::Eq: return ints("=");
      case K::Neq: return Not(ints("="));
      case K::And: return bools("and");
      case K::Or: return bools("or");
      case K::Implies: return bools("=>");
      default: break;
      }
    }
    return Not(Eq(I(e, pre, sub), num(0)));
  }

  T spec(ASTExpr *e, const Sub *sub = nullptr, const Sub *olds = nullptr) {
    auto saved = oldSub;
    oldSub = olds;
    auto t = B(e, nullptr, sub);
    oldSub = saved;
    return t;
  }
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

  std::string name(const std::string &x) const {
    auto it = renames.find(x);
    return it == renames.end() ? x : it->second;
  }

  G havoc(std::vector<std::string> xs, int line, std::string note) {
    G g;
    g.kind = G::Havoc;
    for (auto &x : xs) g.sorts.push_back(proc_.sortOf(x));
    g.vars = std::move(xs);
    g.line = line;
    g.note = std::move(note);
    return g;
  }

  // The assume that starts a branch; `cond` is the source condition.
  static G guard(T p, T cond, int line, std::string note) {
    G g = gAssume(std::move(p), line, std::move(note), true);
    g.src = std::move(cond);
    return g;
  }

  G assertion(T p, int line, std::string what) {
    G g;
    g.kind = G::Assert;
    g.p = std::move(p);
    g.id = (int)obs.size();
    g.line = line;
    g.note = what;
    obs.push_back({line, std::move(what)});
    return g;
  }

  // ---- statements ---------------------------------------------------------

  G stmt(ASTStmt *s) {
    int line = s->getLine();
    if (auto *proof = dynamic_cast<ASTProofStmt *>(s)) {
      // use L(args): a call of lemma L, for its contract alone.
      if (proof->getKind() == ASTProofStmt::Kind::Use) {
        auto *call = dynamic_cast<ASTFunAppExpr *>(proof->getExpr());
        auto *fv = call ? dynamic_cast<ASTVariableExpr *>(call->getFunction()) : nullptr;
        if (!fv || !fns.count(fv->getName()) || !fns[fv->getName()]->isLemma())
          throw Unsupported(where(s) + "use applies only a lemma");
        // A use is erased, so it may not change anything the code can see.
        for (size_t k = 0; k < call->getActuals().size(); ++k)
          if (writes.isWritten(fv->getName(), (int)k))
            throw Unsupported(where(s) + "a lemma may not write through a borrow");
        std::vector<G> cs;
        callExpr(call, cs, "");
        return gSeq(cs);
      }
      auto P = spec(proof->getExpr());
      if (proof->getKind() == ASTProofStmt::Kind::Assume) return gAssume(P, line, "assume");
      return assertion(P, line, "assertion");
    }
    if (auto *blk = dynamic_cast<ASTBlockStmt *>(s)) {
      std::vector<G> cs;
      for (auto *t : blk->getStmts()) cs.push_back(stmt(t));
      return gSeq(cs);
    }
    // Destruction frees memory; to the verifier, values are not memory.
    if (dynamic_cast<ASTDestroyStmt *>(s) || dynamic_cast<ASTDeclStmt *>(s)) return gSeq({});
    // x = E, *x = E, **x = E: all assign x's value.
    if (auto *a = dynamic_cast<ASTAssignStmt *>(s)) {
      auto root = rootOf(a->getLHS());
      if (root.empty()) throw Unsupported(where(s) + "unsupported assignment target");
      auto x = name(root);
      std::vector<G> cs;
      if (auto *call = dynamic_cast<ASTFunAppExpr *>(a->getRHS())) {
        T r = callExpr(call, cs, x);
        if (!(r->kind == Term::Var && r->name == x)) cs.push_back(gAssign(x, r, line));
        return gSeq(cs);
      }
      auto e = I(a->getRHS(), &cs, nullptr);
      cs.push_back(gAssign(x, e, line));
      return gSeq(cs);
    }
    // if (c) S1 else S2  ==>  { assume c; S1 } [] { assume !c; S2 }
    if (auto *i = dynamic_cast<ASTIfStmt *>(s)) {
      std::vector<G> cs;
      auto c = B(i->getCondition(), &cs, nullptr);
      std::vector<G> t{guard(c, c, line, "then")};
      t.push_back(stmt(i->getThen()));
      std::vector<G> f{guard(Not(c), c, line, "else")};
      if (i->getElse()) f.push_back(stmt(i->getElse()));
      cs.push_back(gChoice(gSeq(t), gSeq(f), line));
      return gSeq(cs);
    }
    // case E of { A1 -> S1; A2 -> S2; ... }  ==>
    //   { assume E matches A1; binders := parts of E; S1 }
    //   [] { assume !(E matches A1); { assume E matches A2; ... } [] ... }
    // If no arm matches, nothing happens.
    if (auto *c = dynamic_cast<ASTCaseStmt *>(s)) {
      std::vector<G> cs;
      T t = I(c->getCaseExpr(), &cs, nullptr);
      auto arms = c->getArms();
      G rest = gSeq({});
      for (int k = (int)arms.size() - 1; k >= 0; --k) {
        auto *arm = arms[k];
        std::vector<std::pair<ASTDeclNode *, T>> binds;
        T m = matchCtor(arm->getTag(), arm->getPatterns(), t, binds);
        auto saved = renames;
        std::vector<G> body{guard(m, m, arm->getLine(), "case " + showArm(arm))};
        for (auto &[d, v] : binds) {
          auto b = d->getName();
          auto bs = sortOfTerm(v, sortLookup);
          std::string target = b;
          if (proc_.sorts.count(b) && proc_.sorts[b] != bs) target = b + "#" + std::to_string(fresh++);
          proc_.sorts[target] = bs;
          renames[b] = target;
          body.push_back(gAssign(target, v, arm->getLine(), "bind " + b));
        }
        body.push_back(stmt(arm->getBody()));
        renames = saved;
        rest = gChoice(gSeq(body), gSeq({guard(Not(m), m, arm->getLine(), "not " + showArm(arm)), rest}),
                       arm->getLine());
      }
      cs.push_back(rest);
      return gSeq(cs);
    }
    // while (c) invariant J decreases D S  ==>
    //   assert J; havoc x...; assume J;
    //   { assume c; m := D; S; assert J; assert 0 <= m && D < m; assume false }
    //   [] { assume !c }
    // where x... are the variables S may change.
    if (auto *w = dynamic_cast<ASTWhileStmt *>(s)) {
      std::vector<T> inv;
      for (auto *e : w->getInvariants()) inv.push_back(spec(e));
      T J = And(inv);
      std::vector<G> cs{assertion(J, line, "loop invariant holds on entry")};
      std::vector<std::string> mods;
      for (auto &x : writes.assigned(w->getBody())) mods.push_back(name(x));
      if (!mods.empty()) cs.push_back(havoc(mods, line, "an arbitrary iteration"));
      cs.push_back(gAssume(J, line, "loop invariant"));
      auto c = B(w->getCondition(), &cs, nullptr);
      std::vector<G> once{guard(c, c, line, "loop body")};
      std::string m;
      if (auto *D = w->getDecreases()) {
        m = "measure#" + std::to_string(fresh++);
        once.push_back(gAssign(m, measure(D), line, "measure at the start of the body"));
      }
      once.push_back(stmt(w->getBody()));
      once.push_back(assertion(J, line, "loop invariant is preserved"));
      if (auto *D = w->getDecreases())
        once.push_back(assertion(And({op("<=", {num(0), var(m)}), op("<", {measure(D), var(m)})}),
                                 line, "loop measure decreases"));
      once.push_back(gAssume(FALSE, line, "this path ends"));
      cs.push_back(gChoice(gSeq(once), guard(Not(c), c, line, "loop exit"), line));
      return gSeq(cs);
    }
    if (auto *o = dynamic_cast<ASTOutputStmt *>(s)) {
      std::vector<G> cs;
      I(o->getArg(), &cs, nullptr);
      return gSeq(cs);
    }
    if (dynamic_cast<ASTErrorStmt *>(s))
      return gSeq({assertion(FALSE, line, "error statement is unreachable"),
                   gAssume(FALSE, line, "the program stops")});
    throw Unsupported(where(s) + "unsupported statement in verification");
  }

  // A call g(args), against g's contract, not its body:
  //   assert pre_g[args]; havoc r; assume post_g[args, r]
  // and the call's value is r.  A recursive call must also decrease g's
  // measure.  For each borrowed parameter g writes, the caller's variable x
  // gets a new value x' that post_g describes.  `target` is the variable the
  // value is assigned to, if any, so it can receive r directly.
  T callExpr(ASTFunAppExpr *call, std::vector<G> &cs, const std::string &target) {
    int line = call->getLine();
    auto *fv = dynamic_cast<ASTVariableExpr *>(call->getFunction());
    auto *g = fns[fv->getName()];
    auto formals = g->getFormals();
    auto actuals = call->getActuals();
    std::vector<T> args;
    std::vector<std::string> roots;
    for (size_t k = 0; k < actuals.size(); ++k) {
      args.push_back(I(actuals[k], &cs, nullptr));
      roots.push_back(k < formals.size() && TY.isBorrow(formals[k]) ? name(rootOf(actuals[k])) : "");
    }
    Sub at, olds;
    for (size_t k = 0; k < formals.size() && k < args.size(); ++k)
      at[formals[k]->getName()] = olds[formals[k]->getName()] = args[k];

    if (g == current) {
      T ok = FALSE;
      if (auto *D = g->getDecreases()) {
        auto now = measure(D, &at), before = measure(D, &oldAll);
        ok = And({op("<=", {num(0), now}), op("<", {now, before})});
      }
      cs.push_back(assertion(ok, line, g->getDecreases() ? "recursive call decreases the measure"
                                                         : "recursive call needs a decreases clause"));
    }
    // The call as written, with its arguments: a contract is instantiated
    // at each call, so its obligations are named by the call.
    std::vector<std::string> shown;
    for (auto &a : args) shown.push_back(flat(a));
    std::string site = g->getName() + "(" + join(shown) + ")";
    std::vector<T> req;
    for (auto *e : g->getRequires()) req.push_back(spec(e, &at));
    cs.push_back(assertion(And(req), line, "precondition of " + site));

    // The in-out model needs the written places to be distinct: two borrows
    // of one variable, either of them written, is the case it cannot handle.
    std::vector<std::string> outs, outVars;
    bool overlap = false;
    for (size_t k = 0; k < roots.size(); ++k) {
      if (!writes.isWritten(g->getName(), (int)k)) continue;
      if (roots[k].empty())
        throw Unsupported(where(call) + "a written borrow must name a variable");
      for (size_t j = 0; j < roots.size(); ++j)
        overlap = overlap || (j != k && roots[j] == roots[k]);
      if (std::count(outs.begin(), outs.end(), roots[k])) continue;
      auto after = roots[k] + "'";
      proc_.sorts[after] = proc_.sortOf(roots[k]);
      at[formals[k]->getName()] = var(after);
      outs.push_back(roots[k]);
      outVars.push_back(after);
    }

    if (overlap) cs.push_back(assertion(FALSE, line, "borrowed arguments do not overlap"));

    std::set<std::string> fv2;
    for (auto &a : args) freeVars(a, fv2);
    std::string r = target;
    if (r.empty() || fv2.count(r) || std::count(outs.begin(), outs.end(), r))
      r = g->getName() + "#" + std::to_string(fresh++);
    proc_.sorts[r] = TY.fnSort[g->getName()];
    std::vector<std::string> hv{r};
    hv.insert(hv.end(), outVars.begin(), outVars.end());
    cs.push_back(havoc(hv, line, "result of " + site));
    at["result"] = var(r);
    std::vector<T> ens;
    for (auto *e : g->getEnsures()) ens.push_back(spec(e, &at, &olds));
    cs.push_back(gAssume(And(ens), line, "postcondition of " + site));
    for (size_t k = 0; k < outs.size(); ++k)
      cs.push_back(gAssign(outs[k], var(outVars[k]), line, "written through the borrow"));
    return var(r);
  }
};

// ===========================================================================
// Spec functions.  A spec function is pure: no loops, input, output, error,
// or calls to ordinary functions.  With each if and case merged into an ite,
// its body is one term; it becomes an uninterpreted function and a defining
// axiom, triggered on each application.  A recursive call must decrease the
// function's measure: a definition that does not terminate, like
// f(n) = f(n) + 1, can be false, and from it Z3 proves anything.
// ===========================================================================
class SpecFns {
public:
  std::string decls;                         // declare-fun for each
  std::map<std::string, std::string> axioms; // each one's defining axiom
  std::map<std::string, T> lhs, rhs;         // the definition, for --pspec
  struct Check {
    std::string fn;
    Obligation ob;
    std::string query;
    T formula;
  };
  std::vector<Check> checks; // termination obligations
  std::vector<std::string> skipped;

  SpecFns(ASTProgram *p, Lower &low) : low(low) {
    low.onSpecCall = [this](ASTFunction *g, const std::vector<T> &args, int line) {
      if (g == current && !inMeasure) termination(g, args, line);
    };
    low.sortLookup = [this](const std::string &x) {
      auto it = sorts.find(x);
      return it == sorts.end() ? std::string("Int") : it->second;
    };
    for (auto *f : p->getFunctions()) {
      if (!f->isSpec()) continue;
      try {
        current = f;
        env.clear();
        path.clear();
        sorts.clear();
        std::string params, sig;
        std::vector<T> xs;
        for (auto *x : f->getFormals()) {
          auto s = TY.sortOf(x);
          sorts[x->getName()] = s;
          env[x->getName()] = var(x->getName());
          params += "(|" + x->getName() + "| " + s + ")";
          sig += " " + s;
          xs.push_back(var(x->getName()));
        }
        for (auto *d : f->getDeclarations())
          for (auto *v : d->getVars()) {
            auto s = TY.sortOf(v);
            sorts[v->getName()] = s;
            env[v->getName()] = s == "Int" ? num(0) : var("unset!" + f->getName() + "." + v->getName());
          }
        params0 = env;
        auto stmts = f->getStmts();
        for (size_t i = 0; i + 1 < stmts.size(); ++i) stmt(stmts[i]);
        auto *ret = dynamic_cast<ASTReturnStmt *>(stmts.back());
        T body = low.I(ret->getArg(), nullptr, &env);
        T call = app(f->getName(), xs);
        lhs[f->getName()] = call;
        rhs[f->getName()] = body;
        decls += "(declare-fun |" + f->getName() + "| (" + sig + ") " + TY.fnSort[f->getName()] + ")\n";
        // A local of a datatype sort that some path leaves unset.
        std::set<std::string> fv;
        freeVars(body, fv);
        for (auto &u : fv)
          if (u.rfind("unset!", 0) == 0)
            decls += "(declare-const |" + u + "| " + sorts[u.substr(u.rfind('.') + 1)] + ")\n";
        // The trigger tells Z3 to instantiate the axiom once for each
        // application it sees, which unfolds the definition exactly where a
        // proof needs it.
        if (xs.empty())
          axioms[f->getName()] = "(assert " + smt(Eq(call, body)) + ")\n";
        else
          axioms[f->getName()] = "(assert (forall (" + params + ") (! " + smt(Eq(call, body)) +
                                 " :pattern (" + smt(call) + "))))\n";
      } catch (Unsupported &u) {
        skipped.push_back(f->getName() + ": " + u.what());
      }
    }
    low.onSpecCall = nullptr;
  }

private:
  Lower &low;
  ASTFunction *current = nullptr;
  Sub env, params0;
  std::map<std::string, std::string> sorts;
  std::vector<T> path; // the conditions of the enclosing ifs and arms
  bool inMeasure = false;

  void stmt(ASTStmt *s) {
    if (dynamic_cast<ASTProofStmt *>(s))
      throw Unsupported(where(s) + "assert or assume in a spec function");
    if (dynamic_cast<ASTDestroyStmt *>(s) || dynamic_cast<ASTDeclStmt *>(s)) return;
    if (auto *b = dynamic_cast<ASTBlockStmt *>(s)) {
      for (auto *t : b->getStmts()) stmt(t);
      return;
    }
    if (auto *a = dynamic_cast<ASTAssignStmt *>(s)) {
      auto x = rootOf(a->getLHS());
      if (x.empty()) throw Unsupported(where(s) + "unsupported assignment target");
      env[x] = low.I(a->getRHS(), nullptr, &env);
      return;
    }
    if (auto *i = dynamic_cast<ASTIfStmt *>(s)) {
      auto c = low.B(i->getCondition(), nullptr, &env);
      Sub before = env;
      path.push_back(c);
      stmt(i->getThen());
      Sub e1 = env;
      env = before;
      path.back() = Not(c);
      if (i->getElse()) stmt(i->getElse());
      path.pop_back();
      Sub e2 = env;
      env = before;
      for (auto &[x, v0] : before)
        if (smt(e1[x]) != smt(e2[x])) env[x] = op("ite", {c, e1[x], e2[x]});
        else env[x] = e1[x];
      return;
    }
    if (auto *c = dynamic_cast<ASTCaseStmt *>(s)) {
      T t = low.I(c->getCaseExpr(), nullptr, &env);
      Sub before = env;
      std::vector<T> conds, earlier;
      std::vector<Sub> outs;
      for (auto *arm : c->getArms()) {
        std::vector<std::pair<ASTDeclNode *, T>> binds;
        T m = matchCtor(arm->getTag(), arm->getPatterns(), t, binds);
        env = before;
        for (auto &[d, v] : binds) {
          env[d->getName()] = v;
          sorts[d->getName()] = sortOfTerm(v, low.sortLookup);
        }
        auto here = earlier;
        here.push_back(m);
        path.push_back(And(here));
        stmt(arm->getBody());
        path.pop_back();
        outs.push_back(env);
        conds.push_back(m);
        earlier.push_back(Not(m));
      }
      Sub res = before;
      for (int k = (int)outs.size() - 1; k >= 0; --k)
        for (auto &[x, v0] : before)
          if (smt(outs[k][x]) != smt(res[x])) res[x] = op("ite", {conds[k], outs[k][x], res[x]});
      env = res;
      return;
    }
    throw Unsupported(where(s) + "a spec function may not contain this statement");
  }

  // On the path to a recursive call, the measure at the call is smaller than
  // at entry and not negative.
  void termination(ASTFunction *g, const std::vector<T> &args, int line) {
    T P = FALSE;
    std::string what = "recursive call needs a decreases clause";
    if (auto *D = g->getDecreases()) {
      Sub atCall = params0, atEntry = params0;
      auto formals = g->getFormals();
      for (size_t k = 0; k < formals.size(); ++k) atCall[formals[k]->getName()] = args[k];
      inMeasure = true;
      auto now = low.measure(D, &atCall), before = low.measure(D, &atEntry);
      inMeasure = false;
      P = And({op("<=", {num(0), now}), op("<", {now, before})});
      what = "recursive call decreases the measure";
    }
    T F = path.empty() ? P : Implies(And(path), P);
    std::string q;
    for (auto *x : g->getFormals())
      q += "(declare-const |" + x->getName() + "| " + sorts[x->getName()] + ")\n";
    q += "(assert (not " + smt(F) + "))\n";
    checks.push_back({g->getName(), {line, what}, q, F});
  }
};

// ===========================================================================
// Passivization, after Flanagan and Saxe (POPL 2001).
//
// 1. Version every variable: x := e becomes assume x@k == e for a fresh
//    version x@k, so the program no longer assigns anything ("passive").
// 2. havoc x just starts a new version, with nothing assumed about it.
// 3. After a choice, each branch assumes the merged version equals its own,
//    so the code after the join names one version.
// ===========================================================================

struct Passive {
  G prog;
  std::vector<std::string> versions; // declared constants
  std::map<std::string, std::string> sorts;
  std::vector<std::string> entry; // the formals' entry versions
};

class Passivize {
public:
  Passive run(const Proc &p) {
    out = Passive{};
    proc = &p;
    count.clear();
    dead = false;
    Sub env;
    for (auto *x : p.fn->getFormals()) {
      auto v = version(x->getName());
      env[x->getName()] = var(v);
      env["old(" + x->getName() + ")"] = var(v);
      out.entry.push_back(v);
    }
    out.prog = go(p.body, env);
    return out;
  }

private:
  Passive out;
  const Proc *proc = nullptr;
  std::map<std::string, int> count;
  bool dead = false; // the path so far ends in assume false

  std::string version(const std::string &x) {
    auto v = x + "@" + std::to_string(count[x]++);
    out.versions.push_back(v);
    out.sorts[v] = proc->sortOf(x);
    return v;
  }
  T at(const T &t, Sub &env) {
    std::set<std::string> fv;
    freeVars(t, fv);
    for (auto &x : fv)
      if (!env.count(x) && x.rfind("unset!", 0) != 0) env[x] = var(version(x));
    return subst(t, env);
  }

  G go(const G &c, Sub &env) {
    switch (c.kind) {
    case G::Assume: {
      G r = c;
      r.p = at(c.p, env);
      if (isFalse(r.p)) dead = true;
      return r;
    }
    case G::Assert: {
      G r = c;
      r.p = at(c.p, env);
      return r;
    }
    case G::Assign: {
      auto e = at(c.p, env);
      auto v = version(c.x);
      env[c.x] = var(v);
      G r = gAssume(Eq(var(v), e), c.line, c.x + " := " + flat(c.p));
      r.x = v;
      r.src = c.p;
      return r;
    }
    case G::Havoc: {
      G r = c;
      r.vars.clear();
      for (auto &x : c.vars) {
        auto v = version(x);
        env[x] = var(v);
        r.vars.push_back(v);
      }
      r.note = "havoc " + join(c.vars);
      return r;
    }
    case G::Seq: {
      std::vector<G> ks;
      for (auto &k : c.kids) ks.push_back(go(k, env));
      return gSeq(ks);
    }
    case G::Choice: {
      bool before = dead;
      Sub e1 = env, e2 = env;
      dead = false;
      G a = go(c.kids[0], e1);
      bool d1 = dead;
      dead = false;
      G b = go(c.kids[1], e2);
      bool d2 = dead;
      dead = before || (d1 && d2);
      if (d1 || d2) {
        env = d1 ? e2 : e1; // a branch that cannot finish has nothing to merge
        return gChoice(a, b, c.line);
      }
      std::set<std::string> names;
      for (auto &[x, _] : e1) names.insert(x);
      for (auto &[x, _] : e2) names.insert(x);
      std::vector<G> ma{a}, mb{b};
      for (auto &x : names) {
        auto i1 = e1.find(x), i2 = e2.find(x);
        if (i1 != e1.end() && i2 != e2.end() && smt(i1->second) == smt(i2->second)) continue;
        if (x.rfind("old(", 0) == 0) continue;
        auto v = var(version(x));
        if (i1 != e1.end()) ma.push_back(gAssume(Eq(v, i1->second), c.line, "merge " + x));
        if (i2 != e2.end()) mb.push_back(gAssume(Eq(v, i2->second), c.line, "merge " + x));
        env[x] = v;
      }
      return gChoice(gSeq(ma), gSeq(mb), c.line);
    }
    }
    return c;
  }
};

// ===========================================================================
// Verification conditions.  `target` is the obligation checked; every other
// obligation is assumed.  Target -2 checks them all (for --pvc and --pwp).
// ===========================================================================

bool checked(const G &c, int target) { return target == -2 || c.id == target; }

// wp by substitution, over guarded commands:
//   wp(assume P, Q) = P ==> Q       wp(assert P, Q) = P && Q
//   wp(x := E, Q)   = Q[E/x]        wp(havoc x, Q)  = forall x :: Q
//   wp(S1; S2, Q)   = wp(S1, wp(S2, Q))
//   wp(S1 [] S2, Q) = wp(S1, Q) && wp(S2, Q)
// Substitution is written as a let, which Z3 expands.
T wp(const G &c, const T &Q, int target) {
  switch (c.kind) {
  case G::Assume: return isTrue(c.p) ? Q : Implies(c.p, Q);
  case G::Assert: return checked(c, target) ? And({c.p, Q}) : Implies(c.p, Q);
  case G::Assign: return let(c.x, c.p, Q);
  case G::Havoc: return forall(c.vars, c.sorts, Q);
  case G::Seq: {
    T R = Q;
    for (auto it = c.kids.rbegin(); it != c.kids.rend(); ++it) R = wp(*it, R, target);
    return R;
  }
  case G::Choice: return And({wp(c.kids[0], Q, target), wp(c.kids[1], Q, target)});
  }
  return Q;
}

// The whole function: entry values of the formals, then wp of the body.
T wpFunction(const Proc &p, int target) {
  T Q = wp(p.body, TRUE, target);
  auto formals = p.fn->getFormals();
  for (auto it = formals.rbegin(); it != formals.rend(); ++it) {
    std::set<std::string> fv;
    freeVars(Q, fv);
    auto o = "old(" + (*it)->getName() + ")";
    if (fv.count(o)) Q = let(o, var((*it)->getName()), Q);
  }
  return Q;
}

// The VC of a passive command needs no substitution.  For each command:
// ok (its assertions hold) and post (what holds when it finishes).
//   ok(S1; S2) = ok1 && (post1 ==> ok2)      post(S1; S2) = post1 && post2
//   ok(S1 [] S2) = ok1 && ok2                post(S1 [] S2) = post1 || post2
// Each post is named with a define-fun, so it is written once and the VC
// grows linearly with the program.
struct PassiveVC {
  std::vector<std::pair<std::string, T>> defs;
  T ok;
};
class PassiveVCGen {
public:
  PassiveVC run(const G &prog, int target) {
    this->target = target;
    defs.clear();
    auto [ok, post] = vc(prog);
    // Keep only the definitions the VC uses (the last post is never used).
    std::set<std::string> used;
    freeVars(ok, used);
    for (auto it = defs.rbegin(); it != defs.rend(); ++it)
      if (used.count(it->first)) freeVars(it->second, used);
    std::vector<std::pair<std::string, T>> kept;
    for (auto &d : defs)
      if (used.count(d.first)) kept.push_back(d);
    return {kept, ok};
  }

private:
  int target = -1;
  std::vector<std::pair<std::string, T>> defs;
  T name(T t) {
    std::string n = "N!" + std::to_string(defs.size());
    defs.push_back({n, std::move(t)});
    return var(n);
  }
  std::pair<T, T> vc(const G &c) {
    switch (c.kind) {
    case G::Assume: return {TRUE, c.p};
    case G::Assert: return {checked(c, target) ? c.p : TRUE, c.p};
    case G::Assign:
    case G::Havoc: return {TRUE, TRUE};
    case G::Seq: {
      std::vector<T> oks;
      T post = TRUE;
      for (auto &k : c.kids) {
        auto [okk, postk] = vc(k);
        if (!isTrue(okk)) oks.push_back(isTrue(post) ? okk : Implies(post, okk));
        if (isTrue(postk)) continue;
        post = isTrue(post) ? postk : name(And({post, postk}));
      }
      return {And(oks), post};
    }
    case G::Choice: {
      auto [ok1, p1] = vc(c.kids[0]);
      auto [ok2, p2] = vc(c.kids[1]);
      return {And({ok1, ok2}), name(op("or", {p1, p2}))};
    }
    }
    return {TRUE, TRUE};
  }
};

std::string passiveQuery(const Passive &pv, const PassiveVC &vc) {
  std::string q;
  for (auto &v : pv.versions) q += "(declare-const |" + v + "| " + pv.sorts.at(v) + ")\n";
  for (auto &[n, t] : vc.defs) q += "(define-fun |" + n + "| () Bool " + smt(t) + ")\n";
  q += "(assert (not " + smt(vc.ok) + "))\n";
  return q;
}

// ===========================================================================
// A proof outline: each command preceded by its weakest precondition, with
// substitution carried out and the result simplified.
// ===========================================================================

std::vector<std::string> braces(const T &P) {
  auto L = pretty(P, 70);
  const size_t limit = 40;
  if (L.size() > limit) {
    size_t more = L.size() - limit;
    L.resize(limit);
    L.push_back("... (" + std::to_string(more) + " more lines)");
  }
  L.front() = "{ " + L.front();
  for (size_t i = 1; i < L.size(); ++i) L[i] = "  " + L[i];
  L.back() += " }";
  return L;
}

// Node count, stopping at `limit` (shared subterms count each time).
size_t sizeUpTo(const T &t, size_t limit) {
  size_t n = 1;
  for (auto &a : t->args) {
    if (n >= limit) break;
    n += sizeUpTo(a, limit - n);
  }
  return n;
}
// Substitution copies Q into both branches of every choice, so an outline
// can grow exponentially; past this size it shows a placeholder instead.
const size_t OUTLINE_LIMIT = 4000;
T capped(T P) {
  if (sizeUpTo(P, OUTLINE_LIMIT) < OUTLINE_LIMIT) return P;
  return var("<too large to show: over " + std::to_string(OUTLINE_LIMIT) + " nodes>");
}

struct Outline {
  T pre;
  std::vector<std::string> lines;
};
Outline outline(const G &c, const T &Q, int ind = 0) {
  auto prim = [&](T P) {
    P = capped(P);
    Outline o{P, braces(P)};
    for (auto &l : showCmd(c, false, ind)) o.lines.push_back(l);
    return o;
  };
  switch (c.kind) {
  case G::Assume: return prim(simp(Implies(c.p, Q)));
  case G::Assert: return prim(simp(And({c.p, Q})));
  case G::Assign: return prim(simp(subst(Q, {{c.x, c.p}})));
  case G::Havoc: return prim(simp(forall(c.vars, c.sorts, Q)));
  case G::Seq: {
    std::vector<std::vector<std::string>> parts;
    T q = Q;
    for (auto it = c.kids.rbegin(); it != c.kids.rend(); ++it) {
      auto o = outline(*it, q, ind);
      parts.push_back(o.lines);
      q = o.pre;
    }
    Outline o{q, {}};
    for (auto it = parts.rbegin(); it != parts.rend(); ++it)
      o.lines.insert(o.lines.end(), it->begin(), it->end());
    return o;
  }
  case G::Choice: {
    auto a = outline(c.kids[0], Q, ind + 2), b = outline(c.kids[1], Q, ind + 2);
    T P = capped(simp(And({a.pre, b.pre})));
    Outline o{P, braces(P)};
    o.lines.push_back("{");
    for (auto &l : a.lines) o.lines.push_back("  " + l);
    o.lines.push_back("} [] {");
    for (auto &l : b.lines) o.lines.push_back("  " + l);
    o.lines.push_back("}");
    return o;
  }
  }
  return {Q, {}};
}

// ===========================================================================
// Counterexample traces.  Z3's model gives a value to every version; the
// trace follows the passive program along the path the model takes.
// ===========================================================================

struct SX {
  std::string atom;
  std::vector<SX> kids;
  bool list = false;
};
SX parseSX(const std::string &s, size_t &i) {
  while (i < s.size() && isspace((unsigned char)s[i])) ++i;
  SX x;
  if (i >= s.size()) return x;
  if (s[i] == '(') {
    x.list = true;
    ++i;
    while (true) {
      while (i < s.size() && isspace((unsigned char)s[i])) ++i;
      if (i >= s.size() || s[i] == ')') break;
      x.kids.push_back(parseSX(s, i));
    }
    ++i;
    return x;
  }
  if (s[i] == '|') {
    size_t j = s.find('|', i + 1);
    x.atom = s.substr(i, j - i + 1);
    i = j + 1;
    return x;
  }
  size_t j = i;
  while (j < s.size() && !isspace((unsigned char)s[j]) && s[j] != '(' && s[j] != ')') ++j;
  x.atom = s.substr(i, j - i);
  i = j;
  return x;
}
// A model value as text: (- 1) is -1, (|Node| 1 |Leaf| |Leaf|) is
// Node(1, Leaf, Leaf).
std::string valueOf(const SX &x) {
  if (!x.list) {
    auto a = x.atom;
    if (a.size() > 1 && a.front() == '|') a = a.substr(1, a.size() - 2);
    return a;
  }
  if (x.kids.size() == 2 && x.kids[0].atom == "-") return "-" + valueOf(x.kids[1]);
  if (x.kids.empty()) return "()";
  std::string s = valueOf(x.kids[0]) + "(";
  for (size_t i = 1; i < x.kids.size(); ++i) s += (i > 1 ? ", " : "") + valueOf(x.kids[i]);
  return s + ")";
}

std::string baseName(const std::string &version) { return version.substr(0, version.rfind('@')); }

void guards(const G &c, std::vector<T> &out) {
  if (c.kind == G::Assume && c.guard) out.push_back(c.p);
  for (auto &k : c.kids) guards(k, out);
}

class Trace {
public:
  Trace(const std::map<std::string, std::string> &val, int target) : val(val), target(target) {
    for (auto &[k, v] : val) {
      if (k.size() < 3 || k.front() != '|') continue;
      vals[k.substr(1, k.size() - 2)] = var(v);
    }
  }
  std::vector<std::string> lines;

  bool walk(const G &c) {
    switch (c.kind) {
    case G::Assume:
      if (c.guard || c.note.rfind("merge", 0) == 0) return false;
      if (c.src && !c.x.empty()) {
        emit(c.line, c.note, baseName(c.x) + " = " + get("|" + c.x + "|"));
        return false;
      }
      if (isFalse(c.p)) return false;
      emit(c.line, "assume " + c.note, "");
      return false;
    case G::Assert:
      if (c.id != target) return false;
      emit(c.line, "FAILS: " + c.note, "");
      lines.push_back(pad(14) + flat(subst(c.p, vals)));
      return true;
    case G::Havoc: {
      std::vector<std::string> vs;
      for (auto &v : c.vars) vs.push_back(baseName(v) + " = " + get("|" + v + "|"));
      emit(c.line, c.note, join(vs));
      return false;
    }
    case G::Seq:
      for (auto &k : c.kids)
        if (walk(k)) return true;
      return false;
    case G::Choice: {
      const G *g0 = first(c.kids[0]);
      const G *g1 = first(c.kids[1]);
      bool take0 = !(g0 && g0->guard) || get(smt(g0->p)) == "true";
      const G *g = take0 ? g0 : g1;
      if (g && g->guard) {
        bool holds = take0; // the first branch is the one where the condition holds
        emit(g->line, g->note + ": " + flat(g->src) + (holds ? " holds" : " fails"), "");
      }
      return walk(c.kids[take0 ? 0 : 1]);
    }
    default: return false;
    }
  }

private:
  std::map<std::string, std::string> val;
  Sub vals;
  int target;

  static const G *first(const G &c) {
    if (c.kind == G::Seq) return c.kids.empty() ? nullptr : first(c.kids[0]);
    return &c;
  }
  std::string get(const std::string &k) {
    auto it = val.find(k);
    return it == val.end() ? "?" : it->second;
  }
  void emit(int line, const std::string &what, const std::string &vs) {
    std::string s = "line " + std::to_string(line);
    s.resize(10, ' ');
    s += what;
    if (!vs.empty()) {
      if (s.size() < 44) s.resize(44, ' ');
      else s += "   ";
      s += vs;
    }
    lines.push_back("    " + s);
  }
};

const char *PRELUDE =
    "; C-style division: truncates toward zero, unlike SMT-LIB's div\n"
    "(define-fun tdiv ((a Int) (b Int)) Int\n"
    "  (ite (>= a 0) (ite (> b 0) (div a b) (- (div a (- b))))\n"
    "                (ite (> b 0) (- (div (- a) b)) (div (- a) (- b)))))\n";

// A spec function exists only in specifications: code may not mention it.
// The visitor does not enter specifications, so only code is checked.
class SpecUse : public ASTVisitor {
  std::set<std::string> specs, lemmas;
  bool inSpecFn = false;

public:
  explicit SpecUse(ASTProgram *p) {
    for (auto *f : p->getFunctions()) {
      if (f->isSpec()) specs.insert(f->getName());
      if (f->isLemma()) lemmas.insert(f->getName());
    }
  }
  bool visit(ASTFunction *f) override {
    inSpecFn = f->isSpec();
    return true;
  }
  void endVisit(ASTRankExpr *r) override {
    if (!inSpecFn)
      throw SemanticError("rank used in code on line " + std::to_string(r->getLine()) +
                        "; rank exists only in specifications");
  }
  void endVisit(ASTVariableExpr *v) override {
    if (!inSpecFn && specs.count(v->getName()))
      throw SemanticError("spec function " + v->getName() + " used in code on line " +
                          std::to_string(v->getLine()) +
                          "; spec functions are erased and exist only in specifications");
    if (lemmas.count(v->getName()))
      throw SemanticError("lemma " + v->getName() + " used on line " +
                          std::to_string(v->getLine()) +
                          "; a lemma is erased and is applied only with use");
  }
};

void heading(std::ostream &out, const std::string &h) { out << "-- " << h << "\n"; }

} // namespace

void VCGen::checkSpecUse(ASTProgram *program) {
  if (program == nullptr) return;
  SpecUse check(program);
  program->accept(&check);
}

bool VCGen::verify(ASTProgram *program, TypeInference *types, const std::string &smtFile,
                   std::ostream &out, const Options &opt) {
  TY = Types{};
  TY.build(program, types);
  Writes writes(program);
  Lower low(program, writes);
  SpecFns specs(program, low);
  Passivize passivize;
  PassiveVCGen passiveVC;
  for (auto &sk : specs.skipped) out << sk << " (spec function rejected)\n";

  struct Query {
    std::string fn;
    Obligation ob;
    std::string text; // declarations and assertion, without the prelude
    std::string verdict, model;
    std::vector<std::string> trace;
  };

  // Run one query in its own Z3 process.  (In one process, state left by an
  // earlier query, even after push/pop, can make Z3 give up on a later one.)
  // Returns Z3's answer and whatever it printed after it.
  auto run = [&](const std::string &full) -> std::pair<std::string, std::string> {
    std::string one = smtFile + ".one";
    {
      std::ofstream f(one);
      f << full;
    }
    FILE *pipe = popen(("z3 " + one + " 2>&1").c_str(), "r");
    std::vector<std::string> lines;
    char buf[4096];
    while (pipe && fgets(buf, sizeof buf, pipe)) {
      std::string l(buf);
      if (!l.empty() && l.back() == '\n') l.pop_back();
      lines.push_back(l);
    }
    if (pipe) pclose(pipe);
    std::remove(one.c_str());
    std::string rest;
    for (size_t i = 1; i < lines.size(); ++i) rest += lines[i] + "\n";
    return {lines.empty() ? "no answer" : lines[0], rest};
  };
  auto oneLine = [](std::string s) {
    s.erase(std::remove(s.begin(), s.end(), '\n'), s.end());
    return s;
  };
  auto wanted = [&](const std::string &fn) { return opt.fn.empty() || opt.fn == fn; };

  std::string base = std::string(PRELUDE) + "(set-option :timeout 10000)\n" + TY.decls + specs.decls;

  // ---- spec functions -----------------------------------------------------
  if (opt.pspec) {
    for (auto &[fn, l] : specs.lhs) {
      if (!wanted(fn) && !wanted("spec " + fn)) continue;
      out << "== spec " << fn << "\n";
      heading(out, "definition (an axiom, triggered on each application)");
      auto L = pretty(specs.rhs[fn], 66);
      out << "  " << flat(l) << " ==\n";
      for (auto &x : L) out << "    " << x << "\n";
      out << "  " << specs.axioms[fn];
      bool any = false;
      for (auto &c : specs.checks)
        if (c.fn == fn) {
          if (!any) heading(out, "termination");
          any = true;
          out << "  line " << c.ob.line << ": " << c.ob.what << "\n";
          for (auto &x : pretty(c.formula, 70)) out << "    " << x << "\n";
        }
      out << "\n";
    }
  }

  // Definitions a query may need: rank functions always, spec functions
  // only if they terminate (step 2).
  std::map<std::string, std::string> usable = TY.rankAxioms;
  // A query gets only the definitions it mentions (and those they mention).
  // An unneeded quantified definition can keep Z3 from finding a
  // counterexample, so it answers unknown instead of sat.
  auto preludeFor = [&](const std::string &text) {
    std::set<std::string> need;
    std::vector<std::string> work{text};
    while (!work.empty()) {
      auto t = work.back();
      work.pop_back();
      for (auto &[fn, ax] : usable)
        if (!need.count(fn) && t.find("|" + fn + "|") != std::string::npos) {
          need.insert(fn);
          work.push_back(ax);
        }
    }
    std::string p = base;
    for (auto &fn : need) p += usable[fn];
    return p;
  };

  // 1. Termination of spec functions, using no spec definitions at all.
  std::vector<Query> queries;
  std::set<std::string> unsound;
  for (auto &c : specs.checks) {
    Query q{"spec " + c.fn, c.ob, c.query + "(check-sat)\n"};
    auto [v, rest] = run(preludeFor(q.text) + q.text);
    q.verdict = v;
    if (v != "unsat") unsound.insert(c.fn);
    queries.push_back(q);
  }

  // 2. A spec function's definition is used only if it terminates.
  std::string defs;
  for (auto &[fn, ax] : specs.axioms) {
    if (unsound.count(fn) && !opt.trustSpecs) {
      out << "spec " << fn << ": does not terminate; its definition is not used\n";
      continue;
    }
    if (unsound.count(fn))
      out << "spec " << fn << ": does not terminate, but --trust-specs uses its definition\n";
    usable[fn] = ax;
    defs += ax;
  }

  // 3. Every obligation of every function, one query each.
  for (auto *f : program->getFunctions()) {
    if (f->isSpec() || !wanted(f->getName())) continue;
    Proc proc;
    Passive pv;
    try {
      proc = low.proc(f);
      pv = passivize.run(proc);
    } catch (Unsupported &u) {
      out << f->getName() << ": skipped (" << u.what() << ")\n";
      continue;
    }

    std::string sig = f->getName() + "(";
    for (size_t i = 0; i < f->getFormals().size(); ++i)
      sig += (i ? ", " : "") + f->getFormals()[i]->getName();
    sig += ")";
    bool dumps = opt.pgcl || opt.ppassive || opt.pwp || opt.pvc;
    if (dumps) out << "== " << sig << "\n";
    if (opt.pgcl) {
      heading(out, "guarded commands");
      printG(proc.body, 2, out, false);
    }
    if (opt.ppassive) {
      heading(out, "passive commands: entry versions " + join(pv.entry));
      printG(pv.prog, 2, out, true);
    }
    if (opt.pwp) {
      heading(out, "proof outline: the weakest precondition before each command");
      auto o = outline(proc.body, TRUE);
      for (auto &l : o.lines) out << "  " << l << "\n";
      for (auto &l : braces(TRUE)) out << "  " << l << "\n";
    }
    if (opt.pvc) {
      if (opt.passive) {
        auto vc = passiveVC.run(pv.prog, -2);
        heading(out, "VC (passive), every obligation checked: VC must be valid");
        for (auto &[n, t] : vc.defs) {
          auto L = pretty(t, 66);
          for (size_t i = 0; i < L.size(); ++i)
            out << "  " << (i ? pad(n.size() + 4) : n + " := ") << L[i] << "\n";
        }
        auto L = pretty(vc.ok, 66);
        for (size_t i = 0; i < L.size(); ++i) out << "  " << (i ? "    " : "VC: ") << L[i] << "\n";
      } else {
        heading(out, "VC (wp), every obligation checked: it must be valid");
        for (auto &l : pretty(wpFunction(proc, -2), 74)) out << "  " << l << "\n";
      }
    }
    if (dumps) out << "\n";

    std::vector<std::string> model;
    for (size_t k = 0; k < proc.obs.size(); ++k) {
      std::string t;
      if (opt.passive) {
        t = passiveQuery(pv, passiveVC.run(pv.prog, (int)k));
        model = pv.entry;
      } else {
        model.clear();
        for (auto *p : f->getFormals()) {
          t += "(declare-const |" + p->getName() + "| " + proc.sortOf(p->getName()) + ")\n";
          model.push_back(p->getName());
        }
        t += "(assert (not " + smt(wpFunction(proc, (int)k)) + "))\n";
      }
      t += "(check-sat)\n";
      if (!model.empty()) {
        t += "(get-value (";
        for (auto &m : model) t += "|" + m + "| ";
        t += "))\n";
      }
      Query q{f->getName(), proc.obs[k], t};
      std::string full = preludeFor(q.text) + q.text;
      if (opt.psmt && (opt.psmtLine == 0 || opt.psmtLine == q.ob.line))
        out << "; ==== " << f->getName() << " #" << k << ", line " << q.ob.line << ": "
            << q.ob.what << "\n"
            << full << "\n";
      auto [v, rest] = run(full);
      q.verdict = v;
      if (v != "unsat") {
        size_t i = 0;
        SX vals = parseSX(rest, i);
        std::vector<std::string> shown;
        for (size_t j = 0; j < vals.kids.size() && j < model.size(); ++j)
          if (vals.kids[j].kids.size() == 2)
            shown.push_back(baseName(model[j]) + " = " + valueOf(vals.kids[j].kids[1]));
        q.model = shown.empty() ? oneLine(rest) : join(shown);
      }

      // A trace for a failure comes from the passive query (the obligations
      // are the same in both modes), asked for every version and every
      // branch condition.
      if (opt.pcex && v == "sat") {
        std::vector<T> gs;
        guards(pv.prog, gs);
        std::vector<std::string> asked;
        for (auto &x : pv.versions) asked.push_back("|" + x + "|");
        for (auto &g : gs) asked.push_back(smt(g));
        std::string tq = passiveQuery(pv, passiveVC.run(pv.prog, (int)k)) + "(check-sat)\n";
        if (!asked.empty()) tq += "(get-value (" + join(asked, " ") + "))\n";
        auto [tv, trest] = run(preludeFor(tq) + tq);
        if (tv == "sat") {
          size_t i = 0;
          SX vals = parseSX(trest, i);
          std::map<std::string, std::string> val;
          for (size_t j = 0; j < vals.kids.size() && j < asked.size(); ++j)
            if (vals.kids[j].kids.size() == 2) val[asked[j]] = valueOf(vals.kids[j].kids[1]);
          Trace tr(val, (int)k);
          std::vector<std::string> vs;
          for (auto &e : pv.entry) vs.push_back(baseName(e) + " = " + val["|" + e + "|"]);
          if (!vs.empty()) tr.lines.push_back("    entry     " + join(vs));
          tr.walk(pv.prog);
          q.trace = tr.lines;
        }
      }
      queries.push_back(q);
    }
  }

  // The same queries as one readable SMT-LIB file.
  std::ofstream smtOut(smtFile);
  smtOut << base;
  for (auto &[r, ax] : TY.rankAxioms) smtOut << ax;
  for (auto &q : queries)
    if (q.fn.rfind("spec ", 0) == 0)
      smtOut << "\n; " << q.fn << ", line " << q.ob.line << ": " << q.ob.what << "\n(push)\n"
             << q.text << "(pop)\n";
  smtOut << "\n; definitions of the spec functions that terminate\n" << defs;
  for (auto &q : queries)
    if (q.fn.rfind("spec ", 0) != 0)
      smtOut << "\n; " << q.fn << ", line " << q.ob.line << ": " << q.ob.what << "\n(push)\n"
             << q.text << "(pop)\n";
  auto bytes = smtOut.tellp();
  smtOut.close();
  out << "VC (" << (opt.passive ? "passive" : "wp") << "): " << queries.size() << " queries, "
      << bytes << " bytes in " << smtFile << "\n";

  std::stable_sort(queries.begin(), queries.end(),
                   [](auto &x, auto &y) { return x.fn == y.fn && x.ob.line < y.ob.line; });
  bool allOk = true;
  std::string fn;
  for (auto &r : queries) {
    bool ok = r.verdict == "unsat";
    allOk = allOk && ok;
    if (r.fn.rfind("spec ", 0) == 0 && !wanted(r.fn.substr(5)) && !wanted(r.fn)) continue;
    if (r.fn != fn) {
      fn = r.fn;
      out << fn << ":\n";
    }
    out << "  " << (ok ? "proved " : (r.verdict == "sat" ? "FAILED " : "UNKNOWN")) << " line "
        << r.ob.line << ": " << r.ob.what;
    if (!ok && r.trace.empty() && !r.model.empty() && r.model.find("error") == std::string::npos)
      out << "   when " << r.model;
    if (r.verdict != "sat" && r.verdict != "unsat" && r.verdict != "unknown")
      out << "   (z3: " << r.verdict << ")";
    out << "\n";
    for (auto &l : r.trace) out << l << "\n";
  }
  return allOk;
}

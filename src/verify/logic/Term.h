#pragma once

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace verutop {

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

inline T make(Term t) { return std::make_shared<const Term>(std::move(t)); }
inline T num(long long n) {
  Term t;
  t.num = n;
  return make(t);
}
inline T var(const std::string &x) {
  Term t;
  t.kind = Term::Var;
  t.name = x;
  return make(t);
}
inline T mk(Term::Kind k, const std::string &n, std::vector<T> a) {
  Term t;
  t.kind = k;
  t.name = n;
  t.args = std::move(a);
  return make(t);
}
inline T op(const std::string &o, std::vector<T> a = {}) { return mk(Term::Op, o, std::move(a)); }
// An application of a spec function.
inline T app(const std::string &f, std::vector<T> a) { return mk(Term::Fn, f, std::move(a)); }
inline T ctor(const std::string &tag, std::vector<T> a) { return mk(Term::Ctor, tag, std::move(a)); }
inline T is(const std::string &tag, T x) { return mk(Term::Is, tag, {std::move(x)}); }
inline T sel(const std::string &s, T x) { return mk(Term::Sel, s, {std::move(x)}); }
inline T let(const std::string &x, T v, T body) { return mk(Term::Let, x, {std::move(v), std::move(body)}); }
inline T forall(std::vector<std::string> xs, std::vector<std::string> sorts, T body) {
  if (xs.empty()) return body;
  Term t;
  t.kind = Term::Forall;
  t.bound = std::move(xs);
  t.sorts = std::move(sorts);
  t.args = {std::move(body)};
  return make(t);
}
inline T rebuild(const T &t, std::vector<T> a) {
  Term c = *t;
  c.args = std::move(a);
  return make(c);
}

inline const T TRUE = op("true"), FALSE = op("false");
inline bool isOp(const T &t, const char *o) { return t->kind == Term::Op && t->name == o; }
inline bool isTrue(const T &t) { return isOp(t, "true"); }
inline bool isFalse(const T &t) { return isOp(t, "false"); }

inline T And(const std::vector<T> &all) {
  std::vector<T> ps;
  for (auto &p : all)
    if (!isTrue(p)) ps.push_back(p);
  if (ps.empty()) return TRUE;
  if (ps.size() == 1) return ps[0];
  return op("and", ps);
}
inline T Not(T p) { return op("not", {std::move(p)}); }
inline T Implies(T p, T q) { return op("=>", {std::move(p), std::move(q)}); }
inline T Eq(T a, T b) { return op("=", {std::move(a), std::move(b)}); }

using Sub = std::map<std::string, T>;

// t with each free variable in m replaced; bound variables are left alone.
T subst(const T &t, const Sub &m);
void freeVars(const T &t, std::set<std::string> &out, const std::set<std::string> &bound = {});
// For people only: fold constants and drop trivial connectives.  What Z3
// sees is never simplified.
T simp(const T &t);

} // namespace verutop

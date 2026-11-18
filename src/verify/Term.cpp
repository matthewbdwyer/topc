#include "Term.h"

#include "TermPrint.h"

#include <functional>

namespace verutop {

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

void freeVars(const T &t, std::set<std::string> &out, const std::set<std::string> &bound) {
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

} // namespace verutop

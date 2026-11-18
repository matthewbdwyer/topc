#pragma once

// Small guarded-command programs built by hand, and a brute-force
// evaluator for formulas over a small integer domain, so that tests can
// say whether a verification condition is valid without a solver.
#include "Gcl.h"
#include "Lowering.h"
#include "TermPrint.h"

#include <functional>
#include <map>
#include <sstream>
#include <stdexcept>

namespace vtest {
using namespace verutop;

inline T v(const std::string &x) { return var(x); }
inline T bin(const char *o, T a, T b) { return op(o, {a, b}); }

inline Assert check(T p, int id, int line = 1, std::string note = "check") {
  Assert a;
  a.p = std::move(p);
  a.id = id;
  a.line = line;
  a.note = std::move(note);
  return a;
}
inline Assume guard(T p, int line = 1) {
  Assume a = gAssume(p, line, "branch", true);
  a.src = p;
  return a;
}
// if (c) A else B
inline G ifElse(T c, G a, G b) { return gChoice(gSeq({guard(c), a}), gSeq({guard(Not(c)), b}), 1); }

inline Proc procOf(std::vector<std::string> formals, G body, int obligations) {
  Proc p;
  p.formals = std::move(formals);
  p.body = std::move(body);
  for (int i = 0; i < obligations; ++i) p.obs.emplace_back();
  return p;
}

inline std::string text(const G &g, bool passive = false) {
  std::ostringstream o;
  printG(g, 0, o, passive);
  return o.str();
}

// The value of a formula; a Bool is 0 or 1.  Quantifiers range over SMALL.
inline const std::vector<long long> SMALL{-2, -1, 0, 1, 2, 3};
using Env = std::map<std::string, long long>;
inline long long eval(const T &t, Env &env, const std::map<std::string, T> &defs = {}) {
  auto e = [&](const T &x) { return eval(x, env, defs); };
  switch (t->kind) {
  case verutop::Term::Num: return t->num;
  case verutop::Term::Var: {
    auto it = env.find(t->name);
    if (it != env.end()) return it->second;
    auto d = defs.find(t->name);
    if (d != defs.end()) return e(d->second);
    throw std::runtime_error("unbound " + t->name);
  }
  case verutop::Term::Let: {
    long long val = e(t->args[0]);
    Env inner = env;
    inner[t->name] = val;
    return eval(t->args[1], inner, defs);
  }
  case verutop::Term::Forall: {
    std::function<bool(size_t, Env &)> all = [&](size_t i, Env &en) {
      if (i == t->bound.size()) return eval(t->args[0], en, defs) != 0;
      for (long long d : SMALL) {
        en[t->bound[i]] = d;
        if (!all(i + 1, en)) return false;
      }
      return true;
    };
    Env inner = env;
    return all(0, inner);
  }
  case verutop::Term::Op: break;
  default: throw std::runtime_error("eval: datatypes are not supported");
  }
  auto &o = t->name;
  auto &a = t->args;
  if (o == "true") return 1;
  if (o == "false") return 0;
  if (o == "not") return !e(a[0]);
  if (o == "and") {
    for (auto &x : a)
      if (!e(x)) return 0;
    return 1;
  }
  if (o == "or") {
    for (auto &x : a)
      if (e(x)) return 1;
    return 0;
  }
  if (o == "=>") return !e(a[0]) || e(a[1]);
  if (o == "ite") return e(a[0]) ? e(a[1]) : e(a[2]);
  long long l = e(a[0]), r = e(a[1]);
  if (o == "+") return l + r;
  if (o == "-") return l - r;
  if (o == "*") return l * r;
  if (o == "=") return l == r;
  if (o == "<") return l < r;
  if (o == "<=") return l <= r;
  if (o == ">") return l > r;
  if (o == ">=") return l >= r;
  throw std::runtime_error("eval: operator " + o);
}

// Whether a formula holds for every value of `vars` in SMALL.
inline bool valid(const T &t, const std::vector<std::string> &vars, const std::map<std::string, T> &defs = {}) {
  Env env;
  std::function<bool(size_t)> all = [&](size_t i) {
    if (i == vars.size()) return eval(t, env, defs) != 0;
    for (long long d : SMALL) {
      env[vars[i]] = d;
      if (!all(i + 1)) return false;
    }
    return true;
  };
  return all(0);
}
} // namespace vtest

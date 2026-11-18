#include "Passivize.h"

#include "Support.h"
#include "TermPrint.h"

namespace verutop {

namespace {
class Passivize {
public:
  Passive run(const Proc &p) {
    out = Passive{};
    proc = &p;
    count.clear();
    dead = false;
    Sub env;
    for (auto &x : p.formals) {
      auto v = version(x);
      env[x] = var(v);
      env["old(" + x + ")"] = var(v);
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
    return match(
        c,
        [&](const Assume &a) -> G {
          Assume r = a;
          r.p = at(a.p, env);
          if (isFalse(r.p)) dead = true;
          return r;
        },
        [&](const Assert &a) -> G {
          Assert r = a;
          r.p = at(a.p, env);
          return r;
        },
        [&](const Assign &a) -> G {
          auto e = at(a.e, env);
          auto v = version(a.x);
          env[a.x] = var(v);
          Assume r = gAssume(Eq(var(v), e), a.line, a.x + " := " + flat(a.e));
          r.defines = v;
          r.src = a.e;
          return r;
        },
        [&](const Havoc &h) -> G {
          Havoc r = h;
          r.vars.clear();
          for (auto &x : h.vars) {
            auto v = version(x);
            env[x] = var(v);
            r.vars.push_back(v);
          }
          r.note = "havoc " + join(h.vars);
          return r;
        },
        [&](const Seq &s) -> G {
          std::vector<G> ks;
          for (auto &k : s.kids) ks.push_back(go(k, env));
          return gSeq(ks);
        },
        [&](const Choice &c) -> G {
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
        });
  }
};
} // namespace

Passive passivize(const Proc &p) { return Passivize().run(p); }

} // namespace verutop

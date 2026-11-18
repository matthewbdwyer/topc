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
} // namespace

Passive passivize(const Proc &p) { return Passivize().run(p); }

} // namespace verutop

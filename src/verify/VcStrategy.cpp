#include "VcStrategy.h"

namespace verutop {

bool checked(const Assert &a, int target) { return target == -2 || a.id == target; }

// wp by substitution, over guarded commands:
//   wp(assume P, Q) = P ==> Q       wp(assert P, Q) = P && Q
//   wp(x := E, Q)   = Q[E/x]        wp(havoc x, Q)  = forall x :: Q
//   wp(S1; S2, Q)   = wp(S1, wp(S2, Q))
//   wp(S1 [] S2, Q) = wp(S1, Q) && wp(S2, Q)
// Substitution is written as a let, which Z3 expands.
T wp(const G &c, const T &Q, int target) {
  return match(
      c, [&](const Assume &a) { return isTrue(a.p) ? Q : Implies(a.p, Q); },
      [&](const Assert &a) {
        if (checked(a, target)) return And({a.p, Q});
        return a.missing ? Q : Implies(a.p, Q);
      },
      [&](const Assign &a) { return let(a.x, a.e, Q); },
      [&](const Havoc &h) { return forall(h.vars, h.sorts, Q); },
      [&](const Seq &s) {
        T R = Q;
        for (auto it = s.kids.rbegin(); it != s.kids.rend(); ++it) R = wp(*it, R, target);
        return R;
      },
      [&](const Choice &c) { return And({wp(c.kids[0], Q, target), wp(c.kids[1], Q, target)}); });
}

// The whole function: entry values of the formals, then wp of the body.
T wpFunction(const Proc &p, int target) {
  T Q = wp(p.body, TRUE, target);
  for (auto it = p.formals.rbegin(); it != p.formals.rend(); ++it) {
    std::set<std::string> fv;
    freeVars(Q, fv);
    auto o = "old(" + *it + ")";
    if (fv.count(o)) Q = let(o, var(*it), Q);
  }
  return Q;
}

namespace {
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
  using OkPost = std::pair<T, T>;
  OkPost vc(const G &c) {
    return match(
        c, [&](const Assume &a) { return OkPost{TRUE, a.p}; },
        [&](const Assert &a) { return OkPost{checked(a, target) ? a.p : TRUE, a.missing ? TRUE : a.p}; },
        [&](const Assign &) { return OkPost{TRUE, TRUE}; },
        [&](const Havoc &) { return OkPost{TRUE, TRUE}; },
        [&](const Seq &s) {
          std::vector<T> oks;
          T post = TRUE;
          for (auto &k : s.kids) {
            auto [okk, postk] = vc(k);
            if (!isTrue(okk)) oks.push_back(isTrue(post) ? okk : Implies(post, okk));
            if (isTrue(postk)) continue;
            post = isTrue(post) ? postk : name(And({post, postk}));
          }
          return OkPost{And(oks), post};
        },
        [&](const Choice &c) {
          auto [ok1, p1] = vc(c.kids[0]);
          auto [ok2, p2] = vc(c.kids[1]);
          return OkPost{And({ok1, ok2}), name(op("or", {p1, p2}))};
        });
  }
};
} // namespace

PassiveVC passiveVc(const G &prog, int target) { return PassiveVCGen().run(prog, target); }

} // namespace verutop

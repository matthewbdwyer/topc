#include "VcStrategy.h"

namespace verutop {

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
  case G::Assert:
    if (checked(c, target)) return And({c.p, Q});
    return c.missing ? Q : Implies(c.p, Q);
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
  std::pair<T, T> vc(const G &c) {
    switch (c.kind) {
    case G::Assume: return {TRUE, c.p};
    case G::Assert: return {checked(c, target) ? c.p : TRUE, c.missing ? TRUE : c.p};
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
} // namespace

PassiveVC passiveVc(const G &prog, int target) { return PassiveVCGen().run(prog, target); }

} // namespace verutop

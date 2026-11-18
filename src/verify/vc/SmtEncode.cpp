#include "SmtEncode.h"


namespace verutop {

G flagged(const G &c, int target, bool reach) {
  auto kids = [&](std::vector<G> ks) {
    for (auto &k : ks) k = flagged(k, target, reach);
    return ks;
  };
  return match(
      c,
      [&](const Assume &a) -> G {
        Assume r = a;
        if (a.asmId >= 0) r.p = Implies(var("A!" + std::to_string(a.asmId)), a.p);
        return r;
      },
      [&](const Assert &a) -> G {
        Assert r = a;
        if (a.id == target) {
          if (reach) r.p = FALSE;
        } else if (!a.missing) {
          r.p = Implies(var("O!" + std::to_string(a.id)), a.p);
        }
        return r;
      },
      [&](const Assign &a) -> G { return a; }, [&](const Havoc &h) -> G { return h; },
      [&](const Seq &s) -> G { return Seq{kids(s.kids)}; },
      [&](const Choice &ch) -> G { return Choice{kids(ch.kids), ch.line}; });
}

Query obligationQuery(const Vc &vc) {
  Query q;
  q.consts = vc.consts;
  q.defs = vc.defs;
  q.goal = vc.formula;
  for (auto &x : vc.entry) q.values.push_back(var(x));
  return q;
}

Query traceQuery(const Passive &pv, int target) {
  Query q = obligationQuery(passiveVcOf(pv, pv.prog, target));
  q.values.clear();
  for (auto &x : pv.versions) q.values.push_back(var(x));
  guards(pv.prog, q.values);
  return q;
}

Query dependencyQuery(const Proc &proc, const Passive &pv, int target, bool reach) {
  Query q = obligationQuery(passiveVcOf(pv, flagged(pv.prog, target, reach), target));
  q.values.clear();
  q.core = true;
  for (size_t i = 0; i < proc.assumptions.size(); ++i)
    q.flags.push_back({"A!" + std::to_string(i), "asm!" + std::to_string(i)});
  for (size_t j = 0; j < proc.obs.size(); ++j)
    if ((int)j != target && !proc.obs[j].missing())
      q.flags.push_back({"O!" + std::to_string(j), "ob!" + std::to_string(j)});
  return q;
}

} // namespace verutop

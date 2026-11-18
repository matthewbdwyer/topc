#include "SmtEncode.h"

#include "TermPrint.h"

namespace verutop {

const char *PRELUDE =
    "; C-style division: truncates toward zero, unlike SMT-LIB's div\n"
    "(define-fun tdiv ((a Int) (b Int)) Int\n"
    "  (ite (>= a 0) (ite (> b 0) (div a b) (- (div a (- b))))\n"
    "                (ite (> b 0) (- (div (- a) b)) (div (- a) (- b)))))\n";

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

std::string passiveQuery(const Passive &pv, const PassiveVC &vc) {
  std::string q;
  for (auto &v : pv.versions) q += "(declare-const |" + v + "| " + pv.sorts.at(v) + ")\n";
  for (auto &[n, t] : vc.defs) q += "(define-fun |" + n + "| () Bool " + smt(t) + ")\n";
  q += "(assert (not " + smt(vc.ok) + "))\n";
  return q;
}

} // namespace verutop

#include "SmtEncode.h"

#include "TermPrint.h"

namespace verutop {

const char *PRELUDE =
    "; C-style division: truncates toward zero, unlike SMT-LIB's div\n"
    "(define-fun tdiv ((a Int) (b Int)) Int\n"
    "  (ite (>= a 0) (ite (> b 0) (div a b) (- (div a (- b))))\n"
    "                (ite (> b 0) (- (div (- a) b)) (div (- a) (- b)))))\n";

G flagged(const G &c, int target, bool reach) {
  G r = c;
  switch (c.kind) {
  case G::Assume:
    if (c.asmId >= 0) r.p = Implies(var("A!" + std::to_string(c.asmId)), c.p);
    break;
  case G::Assert:
    if (c.id == target) {
      if (reach) r.p = FALSE;
    } else if (!c.missing) {
      r.p = Implies(var("O!" + std::to_string(c.id)), c.p);
    }
    break;
  case G::Seq:
  case G::Choice:
    for (auto &k : r.kids) k = flagged(k, target, reach);
    break;
  default: break;
  }
  return r;
}

std::string passiveQuery(const Passive &pv, const PassiveVC &vc) {
  std::string q;
  for (auto &v : pv.versions) q += "(declare-const |" + v + "| " + pv.sorts.at(v) + ")\n";
  for (auto &[n, t] : vc.defs) q += "(define-fun |" + n + "| () Bool " + smt(t) + ")\n";
  q += "(assert (not " + smt(vc.ok) + "))\n";
  return q;
}

} // namespace verutop

#include "Query.h"

#include "TermPrint.h"

namespace verutop {

namespace {
const char *PRELUDE =
    "; C-style division: truncates toward zero, unlike SMT-LIB's div\n"
    "(define-fun tdiv ((a Int) (b Int)) Int\n"
    "  (ite (>= a 0) (ite (> b 0) (div a b) (- (div a (- b))))\n"
    "                (ite (> b 0) (- (div (- a) b)) (div (- a) (- b)))))\n";

void symbolsOf(const Query &q, std::set<std::string> &out) {
  for (auto &[n, t] : q.defs) symbols(t, out);
  for (auto &h : q.hypotheses) symbols(h, out);
  if (q.goal) symbols(q.goal, out);
  for (auto &v : q.values) symbols(v, out);
}
} // namespace

std::string Axiom::name() const {
  return symbol.rfind("rank#", 0) == 0 ? "rank!" + symbol.substr(5) : "def!" + symbol;
}

void symbols(const T &t, std::set<std::string> &out) {
  if (t->kind == Term::Fn) out.insert(t->name);
  for (auto &a : t->args) symbols(a, out);
}

std::set<std::string> definitionsNeeded(const Background &bg, const Query &q) {
  std::set<std::string> need;
  if (!q.definitions) return need;
  std::set<std::string> seen;
  symbolsOf(q, seen);
  std::vector<std::string> work(seen.begin(), seen.end());
  while (!work.empty()) {
    auto f = work.back();
    work.pop_back();
    auto it = bg.axioms.find(f);
    if (it == bg.axioms.end() || !need.insert(f).second) continue;
    std::set<std::string> inner;
    symbols(it->second.body, inner);
    for (auto &g : inner)
      if (!need.count(g)) work.push_back(g);
  }
  return need;
}

std::string render(const Axiom &a, bool named) {
  std::string f;
  if (a.params.empty()) {
    f = smt(a.body);
  } else {
    f = "(forall (";
    for (auto &[x, s] : a.params) f += "(|" + x + "| " + s + ")";
    f += ") (! " + smt(a.body) + " :pattern (" + smt(a.trigger) + ")))";
  }
  return named ? "(assert (! " + f + " :named " + a.name() + "))\n" : "(assert " + f + ")\n";
}

std::string renderPrelude(const Background &bg, long long rlimit) {
  std::string p = std::string(PRELUDE) + "(set-option :rlimit " + std::to_string(rlimit) + ")\n" +
                  bg.sorts + bg.functions;
  if (bg.overflow)
    p += "(define-fun INT_MIN () Int (- 9223372036854775808))\n"
         "(define-fun INT_MAX () Int 9223372036854775807)\n";
  return p;
}

std::string renderHeader(const Background &bg, const Query &q) {
  std::string p = renderPrelude(bg, q.rlimit ? q.rlimit : bg.rlimit);
  // (Z3 can also minimize a core, but that search is not bounded by
  // rlimit; the core of the proof it found is small enough.)
  if (q.core) p = "(set-option :produce-unsat-cores true)\n" + p;
  for (auto &f : definitionsNeeded(bg, q)) p += render(bg.axioms.at(f), q.core);
  return p;
}

std::string renderBody(const Query &q) {
  std::string t;
  for (auto &[c, name] : q.flags)
    t += "(declare-const |" + c + "| Bool)\n(assert (! |" + c + "| :named " + name + "))\n";
  for (auto &[x, s] : q.consts) t += "(declare-const |" + x + "| " + s + ")\n";
  for (auto &[n, d] : q.defs) t += "(define-fun |" + n + "| () Bool " + smt(d) + ")\n";
  for (auto &h : q.hypotheses) t += "(assert " + smt(h) + ")\n";
  t += "(assert (not " + smt(q.goal) + "))\n(check-sat)\n";
  if (!q.values.empty()) {
    t += "(get-value (";
    for (auto &v : q.values) t += smt(v) + " ";
    t += "))\n";
  }
  if (q.core) t += "(get-unsat-core)\n";
  return t;
}

std::string render(const Background &bg, const Query &q) { return renderHeader(bg, q) + renderBody(q); }

} // namespace verutop

#include "Outline.h"

#include "TermPrint.h"

namespace verutop {

std::vector<std::string> braces(const T &P) {
  auto L = pretty(P, 70);
  const size_t limit = 40;
  if (L.size() > limit) {
    size_t more = L.size() - limit;
    L.resize(limit);
    L.push_back("... (" + std::to_string(more) + " more lines)");
  }
  L.front() = "{ " + L.front();
  for (size_t i = 1; i < L.size(); ++i) L[i] = "  " + L[i];
  L.back() += " }";
  return L;
}

// Node count, stopping at `limit` (shared subterms count each time).
size_t sizeUpTo(const T &t, size_t limit) {
  size_t n = 1;
  for (auto &a : t->args) {
    if (n >= limit) break;
    n += sizeUpTo(a, limit - n);
  }
  return n;
}
// Substitution copies Q into both branches of every choice, so an outline
// can grow exponentially; past this size it shows a placeholder instead.
const size_t OUTLINE_LIMIT = 4000;
T capped(T P) {
  if (sizeUpTo(P, OUTLINE_LIMIT) < OUTLINE_LIMIT) return P;
  return var("<too large to show: over " + std::to_string(OUTLINE_LIMIT) + " nodes>");
}

Outline outline(const G &c, const T &Q, int ind) {
  auto prim = [&](T P) {
    P = capped(P);
    Outline o{P, braces(P)};
    for (auto &l : showCmd(c, false, ind)) o.lines.push_back(l);
    return o;
  };
  switch (c.kind) {
  case G::Assume: return prim(simp(Implies(c.p, Q)));
  case G::Assert: return prim(simp(And({c.p, Q})));
  case G::Assign: return prim(simp(subst(Q, {{c.x, c.p}})));
  case G::Havoc: return prim(simp(forall(c.vars, c.sorts, Q)));
  case G::Seq: {
    std::vector<std::vector<std::string>> parts;
    T q = Q;
    for (auto it = c.kids.rbegin(); it != c.kids.rend(); ++it) {
      auto o = outline(*it, q, ind);
      parts.push_back(o.lines);
      q = o.pre;
    }
    Outline o{q, {}};
    for (auto it = parts.rbegin(); it != parts.rend(); ++it)
      o.lines.insert(o.lines.end(), it->begin(), it->end());
    return o;
  }
  case G::Choice: {
    auto a = outline(c.kids[0], Q, ind + 2), b = outline(c.kids[1], Q, ind + 2);
    T P = capped(simp(And({a.pre, b.pre})));
    Outline o{P, braces(P)};
    o.lines.push_back("{");
    for (auto &l : a.lines) o.lines.push_back("  " + l);
    o.lines.push_back("} [] {");
    for (auto &l : b.lines) o.lines.push_back("  " + l);
    o.lines.push_back("}");
    return o;
  }
  }
  return {Q, {}};
}

} // namespace verutop

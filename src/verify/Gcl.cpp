#include "Gcl.h"

#include "Support.h"
#include "TermPrint.h"

#include <algorithm>

namespace verutop {

G gAssume(T p, int line, std::string note, bool guard) {
  G g;
  g.kind = G::Assume;
  g.p = std::move(p);
  g.line = line;
  g.note = std::move(note);
  g.guard = guard;
  return g;
}
G gAssign(std::string x, T e, int line, std::string note) {
  G g;
  g.kind = G::Assign;
  g.x = std::move(x);
  g.p = std::move(e);
  g.line = line;
  g.note = std::move(note);
  return g;
}
G gSeq(const std::vector<G> &ks) {
  G g;
  for (auto &k : ks) {
    if (k.kind == G::Seq)
      g.kids.insert(g.kids.end(), k.kids.begin(), k.kids.end());
    else
      g.kids.push_back(k);
  }
  return g;
}
G gChoice(G a, G b, int line) {
  G g;
  g.kind = G::Choice;
  g.kids = {std::move(a), std::move(b)};
  g.line = line;
  return g;
}

std::string withComment(std::string s, const std::string &c, int ind) {
  if (c.empty()) return s;
  size_t col = 44 - std::min(ind, 20);
  if (s.size() < col) s.resize(col, ' ');
  else s += "  ";
  return s + "// " + c;
}

// One primitive command as text lines (a formula may take several).
std::vector<std::string> showCmd(const G &c, bool passive, int ind) {
  std::vector<std::string> out;
  auto formula = [&](const std::string &kw, const T &p, const std::string &comment) {
    auto L = pretty(p, 70 - (int)kw.size());
    for (size_t i = 0; i < L.size(); ++i) {
      std::string s = (i ? pad(kw.size()) : kw) + L[i];
      if (i + 1 == L.size()) s = withComment(s + ";", comment, ind);
      out.push_back(s);
    }
  };
  switch (c.kind) {
  case G::Assume: formula("assume ", c.p, c.note); break;
  case G::Assert:
    formula("assert ", c.p, "#" + std::to_string(c.id) + " line " + std::to_string(c.line) +
                                ": " + c.note);
    break;
  case G::Assign: formula(c.x + " := ", c.p, c.note); break;
  case G::Havoc:
    if (passive)
      out.push_back("// " + c.note + ": " + join(c.vars) + " are new versions");
    else
      out.push_back(withComment("havoc " + join(c.vars) + ";", c.note, ind));
    break;
  default: break;
  }
  return out;
}

void printG(const G &c, int ind, std::ostream &o, bool passive) {
  if (c.kind == G::Seq) {
    for (auto &k : c.kids) printG(k, ind, o, passive);
    return;
  }
  if (c.kind == G::Choice) {
    o << pad(ind) << "{\n";
    printG(c.kids[0], ind + 2, o, passive);
    o << pad(ind) << "} [] {\n";
    printG(c.kids[1], ind + 2, o, passive);
    o << pad(ind) << "}\n";
    return;
  }
  for (auto &l : showCmd(c, passive, ind - 2)) o << pad(ind) << l << "\n";
}

} // namespace verutop

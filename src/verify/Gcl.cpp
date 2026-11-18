#include "Gcl.h"

#include "Support.h"
#include "TermPrint.h"

#include <algorithm>

namespace verutop {

Assume gAssume(T p, int line, std::string note, bool guard) {
  Assume a;
  a.p = std::move(p);
  a.line = line;
  a.note = std::move(note);
  a.guard = guard;
  return a;
}
Assign gAssign(std::string x, T e, int line, std::string note) {
  return Assign{std::move(x), std::move(e), line, std::move(note)};
}
G gSeq(const std::vector<G> &ks) {
  Seq s;
  for (auto &k : ks) {
    if (auto *inner = k.as<Seq>())
      s.kids.insert(s.kids.end(), inner->kids.begin(), inner->kids.end());
    else
      s.kids.push_back(k);
  }
  return s;
}
G gChoice(G a, G b, int line) { return Choice{{std::move(a), std::move(b)}, line}; }

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
  match(
      c, [&](const Assume &a) { formula("assume ", a.p, a.note); },
      [&](const Assert &a) {
        formula("assert ", a.p, "#" + std::to_string(a.id) + " line " + std::to_string(a.line) + ": " + a.note);
      },
      [&](const Assign &a) { formula(a.x + " := ", a.e, a.note); },
      [&](const Havoc &h) {
        if (passive)
          out.push_back("// " + h.note + ": " + join(h.vars) + " are new versions");
        else
          out.push_back(withComment("havoc " + join(h.vars) + ";", h.note, ind));
      },
      [](const Seq &) {}, [](const Choice &) {});
  return out;
}

void printG(const G &c, int ind, std::ostream &o, bool passive) {
  if (auto *s = c.as<Seq>()) {
    for (auto &k : s->kids) printG(k, ind, o, passive);
    return;
  }
  if (auto *ch = c.as<Choice>()) {
    o << pad(ind) << "{\n";
    printG(ch->kids[0], ind + 2, o, passive);
    o << pad(ind) << "} [] {\n";
    printG(ch->kids[1], ind + 2, o, passive);
    o << pad(ind) << "}\n";
    return;
  }
  for (auto &l : showCmd(c, passive, ind - 2)) o << pad(ind) << l << "\n";
}

void guards(const G &c, std::vector<T> &out) {
  match(
      c,
      [&](const Assume &a) {
        if (a.guard) out.push_back(a.p);
      },
      [&](const Seq &s) {
        for (auto &k : s.kids) guards(k, out);
      },
      [&](const Choice &ch) {
        for (auto &k : ch.kids) guards(k, out);
      },
      [](const Assert &) {}, [](const Assign &) {}, [](const Havoc &) {});
}

} // namespace verutop

#include "Feedback.h"

#include "Support.h"
#include "TermPrint.h"

namespace verutop {

SX parseSX(const std::string &s, size_t &i) {
  while (i < s.size() && isspace((unsigned char)s[i])) ++i;
  SX x;
  if (i >= s.size()) return x;
  if (s[i] == '(') {
    x.list = true;
    ++i;
    while (true) {
      while (i < s.size() && isspace((unsigned char)s[i])) ++i;
      if (i >= s.size() || s[i] == ')') break;
      x.kids.push_back(parseSX(s, i));
    }
    ++i;
    return x;
  }
  if (s[i] == '|') {
    size_t j = s.find('|', i + 1);
    x.atom = s.substr(i, j - i + 1);
    i = j + 1;
    return x;
  }
  size_t j = i;
  while (j < s.size() && !isspace((unsigned char)s[j]) && s[j] != '(' && s[j] != ')') ++j;
  x.atom = s.substr(i, j - i);
  i = j;
  return x;
}
// A model value as text: (- 1) is -1, (|Node| 1 |Leaf| |Leaf|) is
// Node(1, Leaf, Leaf).
std::string valueOf(const SX &x) {
  if (!x.list) {
    auto a = x.atom;
    if (a.size() > 1 && a.front() == '|') a = a.substr(1, a.size() - 2);
    return a;
  }
  if (x.kids.size() == 2 && x.kids[0].atom == "-") return "-" + valueOf(x.kids[1]);
  if (x.kids.empty()) return "()";
  std::string s = valueOf(x.kids[0]) + "(";
  for (size_t i = 1; i < x.kids.size(); ++i) s += (i > 1 ? ", " : "") + valueOf(x.kids[i]);
  return s + ")";
}

std::string baseName(const std::string &version) { return version.substr(0, version.rfind('@')); }

void guards(const G &c, std::vector<T> &out) {
  if (c.kind == G::Assume && c.guard) out.push_back(c.p);
  for (auto &k : c.kids) guards(k, out);
}

Trace::Trace(const std::map<std::string, std::string> &val, int target) : val(val), target(target) {
  for (auto &[k, v] : val) {
    if (k.size() < 3 || k.front() != '|') continue;
    vals[k.substr(1, k.size() - 2)] = var(v);
  }
}

bool Trace::walk(const G &c) {
  switch (c.kind) {
  case G::Assume:
    if (c.guard || c.note.rfind("merge", 0) == 0) return false;
    if (c.src && !c.x.empty()) {
      emit(c.line, c.note, baseName(c.x) + " = " + get("|" + c.x + "|"));
      return false;
    }
    if (isFalse(c.p)) return false;
    emit(c.line, "assume " + c.note, "");
    return false;
  case G::Assert:
    if (c.id != target) return false;
    emit(c.line, "FAILS: " + c.note, "");
    lines.push_back(pad(14) + flat(subst(c.p, vals)));
    return true;
  case G::Havoc: {
    std::vector<std::string> vs;
    for (auto &v : c.vars) vs.push_back(baseName(v) + " = " + get("|" + v + "|"));
    emit(c.line, c.note, join(vs));
    return false;
  }
  case G::Seq:
    for (auto &k : c.kids)
      if (walk(k)) return true;
    return false;
  case G::Choice: {
    const G *g0 = first(c.kids[0]);
    const G *g1 = first(c.kids[1]);
    bool take0 = !(g0 && g0->guard) || get(smt(g0->p)) == "true";
    const G *g = take0 ? g0 : g1;
    if (g && g->guard) {
      bool holds = take0; // the first branch is the one where the condition holds
      emit(g->line, g->note + ": " + flat(g->src) + (holds ? " holds" : " fails"), "");
    }
    return walk(c.kids[take0 ? 0 : 1]);
  }
  default: return false;
  }
}

const G *Trace::first(const G &c) {
  if (c.kind == G::Seq) return c.kids.empty() ? nullptr : first(c.kids[0]);
  return &c;
}

std::string Trace::get(const std::string &k) {
  auto it = val.find(k);
  return it == val.end() ? "?" : it->second;
}

void Trace::emit(int line, const std::string &what, const std::string &vs) {
  std::string s = "line " + std::to_string(line);
  s.resize(10, ' ');
  s += what;
  if (!vs.empty()) {
    if (s.size() < 44) s.resize(44, ' ');
    else s += "   ";
    s += vs;
  }
  lines.push_back("    " + s);
}

} // namespace verutop

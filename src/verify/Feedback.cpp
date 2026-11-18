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

Trace::Trace(const std::map<std::string, std::string> &val, int target) : val(val), target(target) {
  for (auto &[k, v] : val) {
    if (k.size() < 3 || k.front() != '|') continue;
    vals[k.substr(1, k.size() - 2)] = var(v);
  }
}

bool Trace::walk(const G &c) {
  return match(
      c,
      [&](const Assume &a) {
        if (a.guard || a.note.rfind("merge", 0) == 0) return false;
        if (!a.defines.empty()) {
          emit(a.line, a.note, baseName(a.defines) + " = " + get("|" + a.defines + "|"));
          return false;
        }
        if (isFalse(a.p)) return false;
        emit(a.line, "assume " + a.note, "");
        return false;
      },
      [&](const Assert &a) {
        if (a.id != target) return false;
        emit(a.line, "FAILS: " + a.note, "");
        lines.push_back(pad(14) + flat(subst(a.p, vals)));
        return true;
      },
      [&](const Assign &) { return false; },
      [&](const Havoc &h) {
        std::vector<std::string> vs;
        for (auto &v : h.vars) vs.push_back(baseName(v) + " = " + get("|" + v + "|"));
        emit(h.line, h.note, join(vs));
        return false;
      },
      [&](const Seq &s) {
        for (auto &k : s.kids)
          if (walk(k)) return true;
        return false;
      },
      [&](const Choice &ch) {
        // Each branch of a choice from an if, a case or a loop starts with
        // its guard; the model says which one holds.
        const Assume *g0 = guardOf(ch.kids[0]);
        const Assume *g1 = guardOf(ch.kids[1]);
        bool take0 = !g0 || get(smt(g0->p)) == "true";
        const Assume *g = take0 ? g0 : g1;
        if (g) {
          bool holds = take0; // the first branch is the one where the condition holds
          emit(g->line, g->note + ": " + flat(g->src) + (holds ? " holds" : " fails"), "");
        }
        return walk(ch.kids[take0 ? 0 : 1]);
      });
}

// The guard a command starts with, if it starts with one.
const Assume *Trace::guardOf(const G &c) {
  if (auto *s = c.as<Seq>()) return s->kids.empty() ? nullptr : guardOf(s->kids[0]);
  auto *a = c.as<Assume>();
  return a && a->guard ? a : nullptr;
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

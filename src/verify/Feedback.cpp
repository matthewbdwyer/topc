#include "Feedback.h"

#include <algorithm>

#include "Support.h"
#include "TermPrint.h"

namespace verutop {

std::string baseName(const std::string &version) { return version.substr(0, version.rfind('@')); }


namespace {

// Follows the passive program along the path the model takes.
class Trace {
public:
  Trace(const std::map<std::string, std::string> &val, int target);
  std::vector<std::string> lines;
  bool walk(const G &c);

private:
  std::map<std::string, std::string> val;
  Sub vals;
  int target;

  static const Assume *guardOf(const G &c);
  std::string get(const std::string &k);
  void emit(int line, const std::string &what, const std::string &vs);
};

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

} // namespace

std::string counterexample(const Answer &a, const std::vector<std::string> &entry) {
  std::vector<std::string> shown;
  for (size_t j = 0; j < a.values.size() && j < entry.size(); ++j)
    shown.push_back(baseName(entry[j]) + " = " + a.values[j]);
  if (!shown.empty()) return join(shown);
  std::string s = a.rest;
  s.erase(std::remove(s.begin(), s.end(), '\n'), s.end());
  return s;
}

std::vector<std::string> trace(const Passive &pv, const Query &asked, const Answer &a, int target) {
  std::map<std::string, std::string> val;
  for (size_t j = 0; j < a.values.size() && j < asked.values.size(); ++j) val[smt(asked.values[j])] = a.values[j];
  Trace tr(val, target);
  std::vector<std::string> vs;
  for (auto &e : pv.entry) vs.push_back(baseName(e) + " = " + val["|" + e + "|"]);
  if (!vs.empty()) tr.lines.push_back("    entry     " + join(vs));
  tr.walk(pv.prog);
  return tr.lines;
}

} // namespace verutop

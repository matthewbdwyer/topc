#include "Trust.h"

#include "Support.h"

#include <cstdlib>

namespace verutop {

bool vacuous(const std::vector<std::string> &reachCore, const std::set<int> &unprovedObligations) {
  for (auto &n : reachCore)
    if (n.rfind("ob!", 0) == 0 && unprovedObligations.count(std::atoi(n.c_str() + 3))) return false;
  return true;
}

namespace {

struct Analysis {
  const VerifyResult &res;
  bool trustSpecs;

  bool verified(const std::string &fn) const {
    auto *f = res.function(fn);
    return f && f->verified();
  }
  std::string whyNot(const std::string &fn) const {
    auto *f = res.function(fn);
    if (!f) return "not checked in this run";
    if (!f->skipped.empty()) return "not checked: " + f->skipped;
    return "not verified";
  }
  bool unsound(const std::string &spec) const {
    for (auto &s : res.specs)
      if (s.name == spec) return !s.terminates;
    return false;
  }
  // A name from an unsat core, as text.
  static std::string show(const FunctionResult &f, const std::string &name) {
    auto n = name.substr(name.find('!') + 1);
    if (name.rfind("def!", 0) == 0) return "definition of " + n;
    if (name.rfind("rank!", 0) == 0) return "rank(" + n + ")";
    size_t i = (size_t)std::atoi(n.c_str());
    if (name.rfind("asm!", 0) == 0 && i < f.proc.assumptions.size()) return f.proc.assumptions[i].what;
    if (name.rfind("ob!", 0) == 0 && i < f.proc.obs.size())
      return "the obligation at line " + std::to_string(f.proc.obs[i].line) + " (" + f.proc.obs[i].what() + ")";
    return name;
  }

  // What every run reports: assumed, checked, not shown to terminate.
  void summary(TrustReport &t) const {
    for (auto &f : res.functions)
      if (f.ran)
        for (auto &a : f.proc.assumptions)
          if (a.kind == Assumption::Assume) t.assumed.push_back(a.what + " in " + f.name);
    for (auto &s : res.specs)
      if (!s.terminates && trustSpecs)
        t.assumed.push_back("definition of " + s.name + ": does not terminate (--trust-specs)");
    // A contract is used by every caller, verified or not.
    std::map<std::string, std::set<std::string>> callers;
    for (auto &[f, g] : res.calls) {
      auto *caller = res.function(f);
      auto ensures = res.hasEnsures.find(g);
      if (f != g && caller && caller->ran && !verified(g) && ensures != res.hasEnsures.end() && ensures->second)
        callers[g].insert(f);
    }
    for (auto &[g, fs] : callers)
      t.assumed.push_back("contract of " + g + " (" + whyNot(g) + "), used by " +
                          join(std::vector<std::string>(fs.begin(), fs.end())));
    int arithmetic = 0;
    for (auto &f : res.functions)
      if (f.ran) arithmetic += f.proc.arithmetic;
    if (arithmetic)
      t.assumed.push_back(std::to_string(arithmetic) + " arithmetic operation" + (arithmetic > 1 ? "s" : "") +
                          " in code: no overflow (--overflow checks " + (arithmetic > 1 ? "them" : "it") + ")");
    for (auto &[r, ok] : res.rankProved)
      if (!ok) t.assumed.push_back("rank(" + r.substr(5) + ") >= 1");

    for (auto &s : res.specs)
      if (s.terminates)
        t.checked.push_back("definition of " + s.name + (s.recursive ? ": terminates" : ": not recursive"));
    for (auto &[r, ok] : res.rankProved)
      if (ok) t.checked.push_back("rank(" + r.substr(5) + ") >= 1: induction step");

    for (auto &f : res.functions)
      if (f.ran)
        for (auto &d : f.proc.diverges) t.diverges.push_back(d == f.name ? f.name : d + " in " + f.name);
    for (auto &f : res.functions)
      for (auto &r : f.obligations) t.anyVacuous = t.anyVacuous || r.vacuous;
  }

  // What each proof used, and what each function's verdict rests on.
  void details(TrustReport &t) const {
    std::map<std::string, std::set<std::string>> rests; // function -> unproved things it rests on
    std::map<std::string, std::set<std::string>> usedCallees;
    std::map<std::string, std::vector<std::string>> lines;
    for (auto &f : res.functions) {
      if (!f.ran) continue;
      auto &assumptions = f.proc.assumptions;
      auto &obs = f.proc.obs;
      for (auto *q : byLine(f)) {
        if (!q->proved()) continue;
        std::string head = "  line " + std::to_string(q->ob.line) + ": " + q->ob.what();
        if (!q->usesKnown) {
          lines[f.name].push_back(head);
          lines[f.name].push_back("      Z3 did not say what this proof depends on");
          rests[f.name].insert("whatever the proof at line " + std::to_string(q->ob.line) + " depends on");
          continue;
        }
        std::vector<std::string> shown;
        for (auto &u : q->uses) {
          size_t i = (size_t)std::atoi(u.substr(u.find('!') + 1).c_str());
          bool isAsm = u.rfind("asm!", 0) == 0 && i < assumptions.size();
          auto kind = isAsm ? assumptions[i].kind : Assumption::Pre;
          // The precondition and the function's own proved obligations are
          // not news, unless they explain why nothing reaches a point.
          bool ownProved = u.rfind("ob!", 0) == 0;
          if (ownProved && i < obs.size())
            for (auto &o : f.obligations)
              if (o.ob.line == obs[i].line && o.ob.what() == obs[i].what() && !o.proved()) ownProved = false;
          if (!q->vacuous && ((isAsm && kind == Assumption::Pre) || ownProved)) continue;
          shown.push_back(show(f, u));
          if (isAsm && kind == Assumption::Assume) rests[f.name].insert(assumptions[i].what);
          if (isAsm && (kind == Assumption::Contract || kind == Assumption::Lemma))
            usedCallees[f.name].insert(assumptions[i].callee);
          if (u.rfind("def!", 0) == 0 && unsound(u.substr(4)))
            rests[f.name].insert("definition of " + u.substr(4) + " (--trust-specs)");
        }
        if (q->vacuous) {
          lines[f.name].push_back(head);
          lines[f.name].push_back("      VACUOUS: nothing reaches line " + std::to_string(q->ob.line) +
                                  (shown.empty() ? ", whatever is assumed" : ", given " + join(shown)));
        } else if (!shown.empty()) {
          lines[f.name].push_back(head);
          lines[f.name].push_back("      uses " + join(shown));
        }
      }
    }
    // A function rests on what its callees rest on: directly, if a callee
    // is not verified; and through each verified callee, on whatever that
    // one rests on.  The second is a fixpoint, since calls can be recursive.
    for (auto &[f, gs] : usedCallees)
      for (auto &g : gs)
        if (g != f && !verified(g)) rests[f].insert("contract of " + g + " (" + whyNot(g) + ")");
    for (bool changed = true; changed;) {
      changed = false;
      for (auto &[f, gs] : usedCallees)
        for (auto &g : gs) {
          if (g == f || !verified(g)) continue;
          for (auto &r : rests[g]) {
            auto via = r.find(" (through ") == std::string::npos ? r + " (through " + g + ")" : r;
            changed = rests[f].insert(via).second || changed;
          }
        }
    }
    for (auto &f : res.functions) {
      if (!f.ran) continue;
      int total = 0, unproved = 0, vacuousCount = 0;
      for (auto &q : f.obligations) {
        ++total;
        unproved += !q.proved();
        vacuousCount += q.vacuous;
      }
      std::string v;
      if (unproved)
        v = "not verified: " + std::to_string(unproved) + " of " + std::to_string(total) +
            (total > 1 ? " obligations" : " obligation") + " not proved";
      else if (rests[f.name].empty())
        v = "verified";
      else
        v = "verified, assuming: " +
            join(std::vector<std::string>(rests[f.name].begin(), rests[f.name].end()), "; ");
      if (vacuousCount)
        v += "; " + std::to_string(vacuousCount) + (vacuousCount > 1 ? " proofs are" : " proof is") + " vacuous";
      t.functions.push_back({f.name, lines[f.name], v});
    }
  }
};

} // namespace

TrustReport analyzeTrust(const VerifyResult &res, bool trustSpecs) {
  TrustReport t;
  Analysis a{res, trustSpecs};
  a.summary(t);
  a.details(t);
  return t;
}

} // namespace verutop

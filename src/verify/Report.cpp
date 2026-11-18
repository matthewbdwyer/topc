#include "Report.h"

#include "Gcl.h"
#include "Outline.h"
#include "Support.h"
#include "TermPrint.h"
#include "VcStrategy.h"

#include <fstream>
#include <tuple>

namespace verutop {

namespace {

void heading(std::ostream &out, const std::string &h) { out << "-- " << h << "\n"; }

struct Printer {
  std::ostream &out;
  const VerifyResult &res;
  const VCGen::Options &opt;

  bool wanted(const std::string &fn) const { return opt.fn.empty() || opt.fn == fn; }
  bool wantedSpec(const std::string &fn) const { return wanted(fn) || wanted("spec " + fn); }
  bool smtLine(int line) const { return opt.psmt && (opt.psmtLine == 0 || opt.psmtLine == line); }

  // ---- --pspec ---------------------------------------------------------------
  void specs() const {
    for (auto &s : res.specs) {
      if (!wantedSpec(s.name)) continue;
      out << "== spec " << s.name << "\n";
      heading(out, "definition (an axiom, triggered on each application)");
      out << "  " << flat(s.lhs) << " ==\n";
      for (auto &x : pretty(s.rhs, 66)) out << "    " << x << "\n";
      out << "  " << render(s.axiom);
      bool any = false;
      for (auto &[fn, c] : res.specChecks)
        if (fn == s.name) {
          if (!any) heading(out, "termination");
          any = true;
          out << "  line " << c.ob.line << ": " << c.ob.what() << "\n";
          for (auto &x : pretty(c.query.goal, 70)) out << "    " << x << "\n";
        }
      out << "\n";
    }
  }

  // ---- one function's IRs, then its queries -----------------------------------
  void function(const FunctionResult &f) const {
    if (!f.ran) {
      out << f.name << ": skipped (" << f.skipped << ")\n";
      return;
    }
    bool dumps = opt.pgcl || opt.ppassive || opt.pwp || opt.pvc;
    if (dumps) out << "== " << f.name << "(" << join(f.proc.formals) << ")\n";
    if (opt.pgcl) {
      heading(out, "guarded commands");
      printG(f.proc.body, 2, out, false);
    }
    if (opt.ppassive) {
      heading(out, "passive commands: entry versions " + join(f.passive.entry));
      printG(f.passive.prog, 2, out, true);
    }
    if (opt.pwp) {
      heading(out, "proof outline: the weakest precondition before each command");
      auto o = outline(f.proc.body, TRUE);
      for (auto &l : o.lines) out << "  " << l << "\n";
      for (auto &l : braces(TRUE)) out << "  " << l << "\n";
    }
    if (opt.pvc) {
      if (opt.passive) {
        auto vc = passiveVc(f.passive.prog, -2);
        heading(out, "VC (passive), every obligation checked: VC must be valid");
        for (auto &[n, t] : vc.defs) {
          auto L = pretty(t, 66);
          for (size_t i = 0; i < L.size(); ++i)
            out << "  " << (i ? pad(n.size() + 4) : n + " := ") << L[i] << "\n";
        }
        auto L = pretty(vc.ok, 66);
        for (size_t i = 0; i < L.size(); ++i) out << "  " << (i ? "    " : "VC: ") << L[i] << "\n";
      } else {
        heading(out, "VC (wp), every obligation checked: it must be valid");
        for (auto &l : pretty(wpFunction(f.proc, -2), 74)) out << "  " << l << "\n";
      }
    }
    if (dumps) out << "\n";

    for (auto &r : f.obligations) {
      std::string id = "; ==== " + f.name + " #" + std::to_string(r.index) + ", line " +
                       std::to_string(r.ob.line) + ": " + r.ob.what();
      if (smtLine(r.ob.line)) out << id << "\n" << render(res.background, r.query) << "\n";
      // --psmt with --ptrust shows the questions about the proof too.
      if (smtLine(r.ob.line) && opt.ptrust)
        for (auto &a : r.asked)
          out << id << (a.reach ? " -- does anything reach it?" : " -- what does the proof use?") << "\n"
              << render(res.background, a.query) << "\n";
    }
  }

  // ---- verdicts ---------------------------------------------------------------
  struct Row {
    std::string fn; // as headed in the report: a function, or "spec f"
    const ObligationResult *r;
    int order; // spec functions first, then functions in program order
    bool shown;
  };
  std::vector<Row> rows() const {
    std::vector<Row> rs;
    for (auto &[fn, r] : res.specChecks) rs.push_back({"spec " + fn, &r, 0, wantedSpec(fn)});
    int order = 0;
    for (auto &f : res.functions) {
      if (!f.ran) continue;
      ++order;
      for (auto &r : f.obligations) rs.push_back({f.name, &r, order, true});
    }
    std::stable_sort(rs.begin(), rs.end(), [](const Row &x, const Row &y) {
      return std::tie(x.order, x.r->ob.line) < std::tie(y.order, y.r->ob.line);
    });
    return rs;
  }
  void verdicts() const {
    auto rs = rows();
    std::string fn;
    for (auto &row : rs) {
      if (!row.shown) continue;
      auto &r = *row.r;
      if (row.fn != fn) {
        fn = row.fn;
        out << fn << ":\n";
      }
      auto v = r.answer.verdict;
      out << "  " << (v == Verdict::Unsat ? "proved " : v == Verdict::Sat ? "FAILED " : "UNKNOWN") << " line "
          << r.ob.line << ": " << r.ob.what();
      if (!r.proved() && r.trace.empty() && !r.counterexample.empty() &&
          r.counterexample.find("error") == std::string::npos)
        out << "   when " << r.counterexample;
      if (v == Verdict::Error) out << "   (z3: " << r.answer.raw << ")";
      if (r.vacuous) out << "   VACUOUS";
      out << "\n";
      for (auto &l : r.trace) out << l << "\n";
    }
    // An UNKNOWN that more work might settle says so, and says how to allow it.
    std::vector<std::string> atLimit;
    for (auto &row : rs)
      if (row.r->answer.limited) atLimit.push_back(row.fn + " line " + std::to_string(row.r->ob.line));
    if (!atLimit.empty())
      out << "Z3 reached its resource limit (" << opt.rlimit << ") on: " << join(atLimit)
          << ". A larger limit may decide " << (atLimit.size() > 1 ? "them" : "it") << ": --vrlimit=N.\n";
  }

  // ---- what the proofs rest on -------------------------------------------------
  void list(const std::string &title, const std::vector<std::string> &items) const {
    if (items.empty()) return;
    out << title << ":\n";
    for (auto &i : items) out << "  " << i << "\n";
  }
  void trust(const TrustReport &t, const std::string &solverVersion) const {
    if (opt.ptrust) {
      out << "\nTrust report\n";
      for (auto &f : t.functions) {
        out << f.name << ":\n";
        for (auto &l : f.lines) out << l << "\n";
        out << "  => " << f.verdict << "\n";
      }
    }
    if (opt.ptrust || !t.assumed.empty() || !t.checked.empty() || !t.diverges.empty() || t.anyVacuous) out << "\n";
    list("Assumed without proof", t.assumed);
    list("Not shown to terminate (decreases *)", t.diverges);
    list("Checked in this run", t.checked);
    if (t.anyVacuous && !opt.ptrust)
      out << "VACUOUS: no execution reaches the obligation, so its proof says nothing "
             "(--ptrust says why)\n";
    if (opt.ptrust)
      list("Always trusted",
           {"topc's parser, type checker, and ownership checks (the encoding relies on ownership)",
            "VERUTOP's translation of the program to verification conditions", solverVersion,
            "erasure, code generation, LLVM, and the runtime: the source is verified, not the binary",
            "input is any integer; output is not modelled"});
  }
};

} // namespace

long long writeSmtFile(const std::string &path, const VerifyResult &res) {
  auto &bg = res.background;
  std::ofstream f(path);
  f << renderPrelude(bg, bg.rlimit);
  for (auto &[symbol, ax] : bg.axioms)
    if (symbol.rfind("rank#", 0) == 0) f << render(ax);
  auto query = [&](const std::string &fn, const ObligationResult &r) {
    f << "\n; " << fn << ", line " << r.ob.line << ": " << r.ob.what() << "\n(push)\n"
      << renderBody(r.query) << "(pop)\n";
  };
  for (auto &[fn, r] : res.specChecks) query("spec " + fn, r);
  f << "\n; definitions of the spec functions that terminate\n";
  for (auto &s : res.specs)
    if (s.used) f << render(s.axiom);
  for (auto &fr : res.functions)
    for (auto &r : fr.obligations) query(fr.name, r);
  return (long long)f.tellp();
}

void printReport(std::ostream &out, const VerifyResult &res, const TrustReport &trust,
                 const VCGen::Options &opt, const std::string &smtFile, long long smtBytes,
                 const std::string &solverVersion) {
  Printer p{out, res, opt};
  for (auto &r : res.rejected) out << r << " (spec function rejected)\n";
  if (opt.pspec) p.specs();
  for (auto &s : res.specs) {
    if (s.terminates) continue;
    if (s.used)
      out << "spec " << s.name << ": does not terminate, but --trust-specs uses its definition\n";
    else
      out << "spec " << s.name << ": does not terminate; its definition is not used\n";
  }
  for (auto &f : res.functions) p.function(f);

  size_t queries = res.specChecks.size();
  for (auto &f : res.functions) queries += f.obligations.size();
  out << "VC (" << (opt.passive ? "passive" : "wp") << "): " << queries << " queries, " << smtBytes
      << " bytes in " << smtFile << "\n";
  p.verdicts();
  p.trust(trust, solverVersion);
}

} // namespace verutop

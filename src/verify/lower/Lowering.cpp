#include "Lowering.h"

#include "Encoding.h"
#include "Support.h"
#include "TermPrint.h"

#include "AST.h"
#include "ASTCtorPattern.h"
#include "ASTOldExpr.h"
#include "ASTProofStmt.h"
#include "ASTRankExpr.h"
#include "ASTVarPattern.h"
#include "ASTVisitor.h"
#include "ASTWildcardPattern.h"
#include "ReferenceMode.h"
#include "ReferenceType.h"
#include "SemanticError.h"
#include "TopFunction.h"
#include "TopInt.h"
#include "TopMu.h"
#include "TopSumType.h"
#include "TypeInference.h"

#include <algorithm>
#include <sstream>
#include <tuple>

namespace verutop {

std::string where(ASTNode *n) { return "line " + std::to_string(n->getLine()) + ": "; }
std::string text(ASTNode *n) {
  std::ostringstream o;
  o << *n;
  return o.str();
}

std::string showArm(ASTCaseArm *a) {
  std::string s = a->getTag();
  auto ps = a->getPatterns();
  if (ps.empty()) return s;
  s += "(";
  for (size_t i = 0; i < ps.size(); ++i) {
    std::ostringstream o;
    o << *ps[i];
    s += (i ? ", " : "") + o.str();
  }
  return s + ")";
}

Lower::Lower(ASTProgram *p, const Encoding &enc, bool overflow) : enc(enc), overflow(overflow) {
  for (auto *f : p->getFunctions()) {
    order.push_back(f);
    fns[f->getName()] = f;
  }
}

CallEdges Lower::calls() {
  CallEdges found;
  edges = &found;
  for (auto *f : order) {
    if (f->isSpec()) continue;
    try {
      proc(f);
    } catch (Unsupported &) {
      // It will be reported as skipped when it is lowered for real.
    }
  }
  edges = nullptr;
  return found;
}

std::string Lower::sortOf(const T &t) const {
  return enc.sortOfTerm(t, [this](const std::string &x) { return scope->sortOf(x); });
}

// A function: assume the precondition; locals start at 0; the body;
// result := the returned value; assert the postcondition.
//
// In ensures, a value formal the body assigns means its entry value,
// old(x): the caller cannot see the change.  A borrowed formal is shared
// with the caller, so *p means its final contents and old(*p) its entry
// contents.
Proc Lower::proc(ASTFunction *f) {
  obs.clear();
  fresh = 0;
  current = f;
  renames.clear();
  proc_ = Proc{};
  proc_.fn = f;
  for (auto *p : f->getFormals()) proc_.formals.push_back(p->getName());
  auto &sorts = proc_.sorts;
  code.proc = &proc_;
  for (auto *p : f->getFormals()) sorts[p->getName()] = enc.sortOf(p);
  for (auto *d : f->getDeclarations())
    for (auto *v : d->getVars()) sorts[v->getName()] = enc.sortOf(v);
  sorts["result"] = enc.resultSort(f->getName());

  std::set<std::string> assigned;
  for (auto *s : f->getStmts()) {
    auto a = enc.assigned(s);
    assigned.insert(a.begin(), a.end());
  }
  old.clear();
  oldAll.clear();
  for (auto *p : f->getFormals()) {
    auto x = p->getName();
    sorts["old(" + x + ")"] = sorts[x];
    if (!assigned.count(x)) continue;
    oldAll[x] = var("old(" + x + ")");
    if (!enc.isBorrow(p)) old[x] = oldAll[x];
  }

  std::vector<G> cs;
  std::vector<T> req;
  for (auto *e : f->getRequires()) req.push_back(spec(e));
  if (f->mayDiverge()) proc_.diverges.push_back(f->getName());
  if (!req.empty())
    cs.push_back(tracked(gAssume(And(req), f->getRequires()[0]->getLine(), "requires"),
                         Assumption::Pre, "the precondition"));
  if (machineInts()) {
    std::vector<T> rs;
    for (auto *p : f->getFormals())
      if (sorts[p->getName()] == "Int") rs.push_back(Encoding::inRange(var(p->getName())));
    if (!rs.empty()) cs.push_back(gAssume(And(rs), f->getLine(), "arguments are 64-bit values"));
  }
  for (auto *d : f->getDeclarations())
    for (auto *v : d->getVars()) {
      if (sorts[v->getName()] == "Int")
        cs.push_back(gAssign(v->getName(), num(0), d->getLine(), "locals start at 0"));
      else
        cs.push_back(havoc({v->getName()}, d->getLine(), "no initial value"));
    }
  auto stmts = f->getStmts();
  for (size_t i = 0; i + 1 < stmts.size(); ++i) cs.push_back(stmt(stmts[i]));
  auto *ret = dynamic_cast<ASTReturnStmt *>(stmts.back());
  std::vector<G> pre;
  T r = I(ret->getArg(), &pre, nullptr);
  cs.insert(cs.end(), pre.begin(), pre.end());
  cs.push_back(gAssign("result", r, ret->getLine(), "return"));
  std::vector<T> ens;
  for (auto *e : f->getEnsures()) ens.push_back(spec(e, &old, &oldAll));
  cs.push_back(assertion(And(ens), ret, ObKind::Postcondition));
  proc_.body = gSeq(cs);
  proc_.obs = obs;
  return proc_;
}

std::string Lower::name(const std::string &x) const {
  auto it = renames.find(x);
  return it == renames.end() ? x : it->second;
}

// An unknown value; an int in code is still a 64-bit value.
G Lower::havoc(std::vector<std::string> xs, int line, std::string note) {
  Havoc h;
  std::vector<T> rs;
  for (auto &x : xs) {
    h.sorts.push_back(proc_.sortOf(x));
    if (h.sorts.back() == "Int") rs.push_back(Encoding::inRange(var(x)));
  }
  h.vars = std::move(xs);
  h.line = line;
  h.note = std::move(note);
  if (!machineInts() || rs.empty()) return h;
  return gSeq({h, gAssume(And(rs), line, "64-bit values")});
}

// The assume that starts a branch; `cond` is the source condition.
Assume Lower::guard(T p, T cond, int line, std::string note) {
  Assume a = gAssume(std::move(p), line, std::move(note), true);
  a.src = std::move(cond);
  return a;
}

// An assume that a proof may be found to depend on.
Assume Lower::tracked(Assume g, Assumption::Kind kind, std::string what, std::string callee) {
  if (isTrue(g.p)) return g;
  g.asmId = (int)proc_.assumptions.size();
  proc_.assumptions.push_back({kind, g.line, std::move(what), std::move(callee)});
  return g;
}

Assert Lower::assertion(T p, ASTNode *at, ObKind kind, std::string subject, std::string callee) {
  Obligation ob;
  ob.kind = kind;
  ob.line = at->getLine();
  ob.column = at->getColumn();
  ob.subject = std::move(subject);
  ob.callee = std::move(callee);
  return assertion(std::move(p), std::move(ob));
}

Assert Lower::assertion(T p, Obligation ob) {
  Assert g;
  g.p = ob.missing() ? FALSE : std::move(p);
  g.id = (int)obs.size();
  g.line = ob.line;
  g.note = ob.what();
  g.missing = ob.missing();
  obs.push_back(std::move(ob));
  return g;
}

bool Lower::machineInts() const { return overflow && !ghost && !current->isLemma(); }

} // namespace verutop

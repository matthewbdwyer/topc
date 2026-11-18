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

// The variable an lvalue or a borrowed argument names: x, *x, **x, &x.
std::string rootOf(ASTExpr *e, int *derefs) {
  int d = 0;
  while (true) {
    if (auto *r = dynamic_cast<ASTDeRefExpr *>(e)) {
      e = r->getPtr();
      ++d;
    } else if (auto *b = dynamic_cast<ASTBorrowExpr *>(e)) {
      e = b->getVar();
    } else {
      break;
    }
  }
  if (derefs) *derefs = d;
  auto *v = dynamic_cast<ASTVariableExpr *>(e);
  return v ? v->getName() : "";
}

Writes::Writes(ASTProgram *p, const Encoding &enc) {
  for (auto *f : p->getFunctions()) fns[f->getName()] = f;
  struct Scan : ASTVisitor {
    std::set<std::string> stores;                              // roots of *... = E
    std::vector<std::tuple<std::string, int, std::string>> passes; // (callee, k, root)
    void endVisit(ASTAssignStmt *a) override {
      int d = 0;
      auto r = rootOf(a->getLHS(), &d);
      if (d > 0) stores.insert(r);
    }
    void endVisit(ASTFunAppExpr *c) override {
      auto *fv = dynamic_cast<ASTVariableExpr *>(c->getFunction());
      if (!fv) return;
      auto as = c->getActuals();
      for (size_t k = 0; k < as.size(); ++k) passes.push_back({fv->getName(), (int)k, rootOf(as[k])});
    }
  };
  std::map<std::string, Scan> scans;
  for (auto *f : p->getFunctions()) f->accept(&scans[f->getName()]);
  bool changed = true;
  while (changed) {
    changed = false;
    for (auto *f : p->getFunctions()) {
      auto formals = f->getFormals();
      auto &sc = scans[f->getName()];
      for (size_t k = 0; k < formals.size(); ++k) {
        if (!enc.isBorrow(formals[k]) || written.count({f->getName(), (int)k})) continue;
        auto x = formals[k]->getName();
        bool w = sc.stores.count(x);
        for (auto &[g, j, r] : sc.passes)
          if (r == x && written.count({g, j})) w = true;
        if (w) {
          written.insert({f->getName(), (int)k});
          changed = true;
        }
      }
    }
  }
}

// The variables a statement may change: assignment targets, and variables
// lent to a parameter that is written.
std::set<std::string> Writes::assigned(ASTNode *s) const {
  struct Scan : ASTVisitor {
    const Writes *w;
    std::set<std::string> out;
    void endVisit(ASTAssignStmt *a) override { out.insert(rootOf(a->getLHS())); }
    void endVisit(ASTFunAppExpr *c) override {
      auto *fv = dynamic_cast<ASTVariableExpr *>(c->getFunction());
      if (!fv) return;
      auto as = c->getActuals();
      for (size_t k = 0; k < as.size(); ++k)
        if (w->isWritten(fv->getName(), (int)k)) out.insert(rootOf(as[k]));
    }
  } scan;
  scan.w = this;
  s->accept(&scan);
  scan.out.erase("");
  return scan.out;
}

T matchCtor(const Encoding &enc, const std::string &tag, const std::vector<ASTPattern *> &subs,
            const T &t, std::vector<std::pair<ASTDeclNode *, T>> &binds) {
  std::vector<T> cs{is(tag, t)};
  auto &fs = enc.fieldsOf(tag);
  for (size_t i = 0; i < subs.size() && i < fs.size(); ++i)
    cs.push_back(matchPattern(enc, subs[i], sel(fs[i].first, t), binds));
  return And(cs);
}
T matchPattern(const Encoding &enc, ASTPattern *p, const T &t,
               std::vector<std::pair<ASTDeclNode *, T>> &binds) {
  if (auto *c = dynamic_cast<ASTCtorPattern *>(p))
    return matchCtor(enc, c->getTag(), c->getSubPatterns(), t, binds);
  if (auto *v = dynamic_cast<ASTVarPattern *>(p)) binds.push_back({v->getDecl(), t});
  return TRUE;
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

Lower::Lower(ASTProgram *p, const Encoding &enc, const Writes &w) : enc(enc), writes(w) {
  for (auto *f : p->getFunctions()) fns[f->getName()] = f;
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
  sortLookup = [this](const std::string &x) { return proc_.sortOf(x); };
  for (auto *p : f->getFormals()) sorts[p->getName()] = enc.sortOf(p);
  for (auto *d : f->getDeclarations())
    for (auto *v : d->getVars()) sorts[v->getName()] = enc.sortOf(v);
  sorts["result"] = enc.resultSort(f->getName());

  std::set<std::string> assigned;
  for (auto *s : f->getStmts()) {
    auto a = writes.assigned(s);
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
      if (sorts[p->getName()] == "Int") rs.push_back(inRange(var(p->getName())));
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
  cs.push_back(assertion(And(ens), ret->getLine(), "postcondition"));
  proc_.body = gSeq(cs);
  proc_.obs = obs;
  return proc_;
}

T Lower::I(ASTExpr *e, std::vector<G> *pre, const Sub *sub) {
  if (auto *n = dynamic_cast<ASTNumberExpr *>(e)) return num(n->getValue());
  if (auto *v = dynamic_cast<ASTVariableExpr *>(e)) {
    if (sub) {
      auto it = sub->find(v->getName());
      if (it != sub->end()) return it->second;
    }
    return var(name(v->getName()));
  }
  // Ownership makes a reference its referent: one owner, no aliases.
  if (auto *a = dynamic_cast<ASTAllocExpr *>(e)) return I(a->getInitializer(), pre, sub);
  if (auto *d = dynamic_cast<ASTDeRefExpr *>(e)) return I(d->getPtr(), pre, sub);
  if (auto *b = dynamic_cast<ASTBorrowExpr *>(e)) return I(b->getVar(), pre, sub);
  if (auto *c = dynamic_cast<ASTSumCtorExpr *>(e)) {
    std::vector<T> as;
    for (auto *a : c->getArgs()) as.push_back(I(a, pre, sub));
    return ctor(c->getTag(), as);
  }
  if (auto *rk = dynamic_cast<ASTRankExpr *>(e)) {
    T t = I(rk->getExpr(), pre, sub);
    auto s = enc.sortOfTerm(t, sortLookup);
    if (!Encoding::isDatatype(s)) throw Unsupported(where(e) + "rank applies only to a sum value");
    return Encoding::rank(t, s);
  }
  if (auto *o = dynamic_cast<ASTOldExpr *>(e)) {
    if (!oldSub) throw Unsupported(where(e) + "old(...) is allowed only in ensures");
    return I(o->getExpr(), pre, oldSub);
  }
  if (dynamic_cast<ASTInputExpr *>(e)) {
    if (!pre) throw Unsupported(where(e) + "input in a specification");
    std::string v = "input#" + std::to_string(fresh++);
    pre->push_back(havoc({v}, e->getLine(), "input"));
    return var(v);
  }
  if (auto *b = dynamic_cast<ASTBinaryExpr *>(e)) {
    using K = ASTBinaryExpr::BinaryOp;
    auto two = [&](const char *o) {
      auto l = I(b->getLeft(), pre, sub);
      auto r = I(b->getRight(), pre, sub);
      return op(o, {l, r});
    };
    // In code the result must be a 64-bit value; a specification, a
    // lemma, and the arguments of `use` compute with mathematical ints.
    auto checked = [&](T t) {
      if (!pre || ghost || current->isLemma()) return t;
      if (overflow)
        pre->push_back(assertion(inRange(t), e->getLine(), flat(t) + " does not overflow"));
      else
        ++proc_.arithmetic;
      return t;
    };
    switch (b->getOpKind()) {
    case K::Add: return checked(two("+"));
    case K::Sub: return checked(two("-"));
    case K::Mul: return checked(two("*"));
    case K::Div: {
      auto t = two("tdiv");
      if (pre)
        pre->push_back(assertion(Not(Eq(t->args[1], num(0))), e->getLine(),
                                 "divisor is not zero"));
      return checked(t);
    }
    default: return op("ite", {B(e, pre, sub), num(1), num(0)});
    }
  }
  if (auto *call = dynamic_cast<ASTFunAppExpr *>(e)) {
    auto *fv = dynamic_cast<ASTVariableExpr *>(call->getFunction());
    if (!fv || !fns.count(fv->getName()))
      throw Unsupported(where(call) + "a call through a function value");
    auto *g = fns[fv->getName()];
    if (pre && g->isLemma())
      throw Unsupported(where(call) + "a lemma is applied only with use");
    if (pre) return callExpr(call, *pre, "");
    if (!g->isSpec())
      throw Unsupported(where(call) + "a specification may call only spec functions");
    std::vector<T> as;
    for (auto *a : call->getActuals()) as.push_back(I(a, nullptr, sub));
    if (onSpecCall) onSpecCall(g, as, call->getLine());
    return app(g->getName(), as);
  }
  throw Unsupported(where(e) + "unsupported expression in verification: " + text(e));
}

// A TOP condition is true when it is not zero.
T Lower::B(ASTExpr *e, std::vector<G> *pre, const Sub *sub) {
  if (auto *b = dynamic_cast<ASTBinaryExpr *>(e)) {
    using K = ASTBinaryExpr::BinaryOp;
    auto ints = [&](const char *o) {
      auto l = I(b->getLeft(), pre, sub);
      auto r = I(b->getRight(), pre, sub);
      return op(o, {l, r});
    };
    auto bools = [&](const char *o) {
      auto l = B(b->getLeft(), pre, sub);
      auto r = B(b->getRight(), pre, sub);
      return op(o, {l, r});
    };
    switch (b->getOpKind()) {
    case K::Gt: return ints(">");
    case K::Lt: return ints("<");
    case K::Ge: return ints(">=");
    case K::Le: return ints("<=");
    case K::Eq: return ints("=");
    case K::Neq: return Not(ints("="));
    case K::And: return bools("and");
    case K::Or: return bools("or");
    case K::Implies: return bools("=>");
    default: break;
    }
  }
  return Not(Eq(I(e, pre, sub), num(0)));
}

T Lower::spec(ASTExpr *e, const Sub *sub, const Sub *olds) {
  auto saved = oldSub;
  oldSub = olds;
  auto t = B(e, nullptr, sub);
  oldSub = saved;
  return t;
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
    if (h.sorts.back() == "Int") rs.push_back(inRange(var(x)));
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

// An obligation that fails because a clause is missing.  It is not a fact
// about the program, so the obligations after it do not assume it.
Assert Lower::missingClause(int line, std::string what) {
  Assert g = assertion(FALSE, line, std::move(what));
  g.missing = true;
  obs.back().missing = true;
  return g;
}

Assert Lower::assertion(T p, int line, std::string what) {
  Assert g;
  g.p = std::move(p);
  g.id = (int)obs.size();
  g.line = line;
  g.note = what;
  obs.push_back({line, std::move(what)});
  obs.back().unreachable = isFalse(g.p);
  return g;
}

G Lower::stmt(ASTStmt *s) {
  int line = s->getLine();
  if (auto *proof = dynamic_cast<ASTProofStmt *>(s)) {
    // use L(args): a call of lemma L, for its contract alone.
    if (proof->getKind() == ASTProofStmt::Kind::Use) {
      auto *call = dynamic_cast<ASTFunAppExpr *>(proof->getExpr());
      auto *fv = call ? dynamic_cast<ASTVariableExpr *>(call->getFunction()) : nullptr;
      if (!fv || !fns.count(fv->getName()) || !fns[fv->getName()]->isLemma())
        throw Unsupported(where(s) + "use applies only a lemma");
      // A use is erased, so it may not change anything the code can see.
      for (size_t k = 0; k < call->getActuals().size(); ++k)
        if (writes.isWritten(fv->getName(), (int)k))
          throw Unsupported(where(s) + "a lemma may not write through a borrow");
      std::vector<G> cs;
      ++ghost;
      callExpr(call, cs, "");
      --ghost;
      return gSeq(cs);
    }
    auto P = spec(proof->getExpr());
    if (proof->getKind() == ASTProofStmt::Kind::Assume)
      return tracked(gAssume(P, line, "assume"), Assumption::Assume,
                     "assume (line " + std::to_string(line) + ")");
    return assertion(P, line, "assertion");
  }
  if (auto *blk = dynamic_cast<ASTBlockStmt *>(s)) {
    std::vector<G> cs;
    for (auto *t : blk->getStmts()) cs.push_back(stmt(t));
    return gSeq(cs);
  }
  // Destruction frees memory; to the verifier, values are not memory.
  if (dynamic_cast<ASTDestroyStmt *>(s) || dynamic_cast<ASTDeclStmt *>(s)) return gSeq({});
  // x = E, *x = E, **x = E: all assign x's value.
  if (auto *a = dynamic_cast<ASTAssignStmt *>(s)) {
    auto root = rootOf(a->getLHS());
    if (root.empty()) throw Unsupported(where(s) + "unsupported assignment target");
    auto x = name(root);
    std::vector<G> cs;
    if (auto *call = dynamic_cast<ASTFunAppExpr *>(a->getRHS())) {
      T r = callExpr(call, cs, x);
      if (!(r->kind == Term::Var && r->name == x)) cs.push_back(gAssign(x, r, line));
      return gSeq(cs);
    }
    auto e = I(a->getRHS(), &cs, nullptr);
    cs.push_back(gAssign(x, e, line));
    return gSeq(cs);
  }
  // if (c) S1 else S2  ==>  { assume c; S1 } [] { assume !c; S2 }
  if (auto *i = dynamic_cast<ASTIfStmt *>(s)) {
    std::vector<G> cs;
    auto c = B(i->getCondition(), &cs, nullptr);
    std::vector<G> t{guard(c, c, line, "then")};
    t.push_back(stmt(i->getThen()));
    std::vector<G> f{guard(Not(c), c, line, "else")};
    if (i->getElse()) f.push_back(stmt(i->getElse()));
    cs.push_back(gChoice(gSeq(t), gSeq(f), line));
    return gSeq(cs);
  }
  // case E of { A1 -> S1; A2 -> S2; ... }  ==>
  //   { assume E matches A1; binders := parts of E; S1 }
  //   [] { assume !(E matches A1); { assume E matches A2; ... } [] ... }
  // If no arm matches, nothing happens.
  if (auto *c = dynamic_cast<ASTCaseStmt *>(s)) {
    std::vector<G> cs;
    T t = I(c->getCaseExpr(), &cs, nullptr);
    auto arms = c->getArms();
    G rest = gSeq({});
    for (int k = (int)arms.size() - 1; k >= 0; --k) {
      auto *arm = arms[k];
      std::vector<std::pair<ASTDeclNode *, T>> binds;
      T m = matchCtor(enc, arm->getTag(), arm->getPatterns(), t, binds);
      auto saved = renames;
      std::vector<G> body{guard(m, m, arm->getLine(), "case " + showArm(arm))};
      for (auto &[d, v] : binds) {
        auto b = d->getName();
        auto bs = enc.sortOfTerm(v, sortLookup);
        std::string target = b;
        if (proc_.sorts.count(b) && proc_.sorts[b] != bs) target = b + "#" + std::to_string(fresh++);
        proc_.sorts[target] = bs;
        renames[b] = target;
        body.push_back(gAssign(target, v, arm->getLine(), "bind " + b));
        if (machineInts() && bs == "Int")
          body.push_back(gAssume(inRange(var(target)), arm->getLine(), "a 64-bit value"));
      }
      body.push_back(stmt(arm->getBody()));
      renames = saved;
      rest = gChoice(gSeq(body), gSeq({guard(Not(m), m, arm->getLine(), "not " + showArm(arm)), rest}),
                     arm->getLine());
    }
    cs.push_back(rest);
    return gSeq(cs);
  }
  // while (c) invariant J decreases D S  ==>
  //   assert J; havoc x...; assume J;
  //   { assume c; m := D; S; assert J; assert 0 <= m && D < m; assume false }
  //   [] { assume !c }
  // where x... are the variables S may change.
  if (auto *w = dynamic_cast<ASTWhileStmt *>(s)) {
    std::vector<T> inv;
    for (auto *e : w->getInvariants()) inv.push_back(spec(e));
    T J = And(inv);
    std::vector<G> cs{assertion(J, line, "loop invariant holds on entry")};
    // Every loop has a measure, or says with `decreases *` that it may
    // run forever.
    if (!w->getDecreases() && !w->mayDiverge())
      cs.push_back(missingClause(line, "loop needs a decreases clause"));
    if (w->mayDiverge()) proc_.diverges.push_back("the loop at line " + std::to_string(line));
    if (w->mayDiverge() && !current->mayDiverge())
      cs.push_back(missingClause(line, "loop may not terminate; " + current->getName() +
                                           " needs decreases *"));
    std::vector<std::string> mods;
    for (auto &x : writes.assigned(w->getBody())) mods.push_back(name(x));
    if (!mods.empty()) cs.push_back(havoc(mods, line, "an arbitrary iteration"));
    cs.push_back(gAssume(J, line, "loop invariant"));
    auto c = B(w->getCondition(), &cs, nullptr);
    std::vector<G> once{guard(c, c, line, "loop body")};
    std::string m;
    if (auto *D = w->getDecreases()) {
      m = "measure#" + std::to_string(fresh++);
      once.push_back(gAssign(m, measure(D), line, "measure at the start of the body"));
    }
    once.push_back(stmt(w->getBody()));
    once.push_back(assertion(J, line, "loop invariant is preserved"));
    if (auto *D = w->getDecreases())
      once.push_back(assertion(And({op("<=", {num(0), var(m)}), op("<", {measure(D), var(m)})}),
                               line, "loop measure decreases"));
    once.push_back(gAssume(FALSE, line, "this path ends"));
    cs.push_back(gChoice(gSeq(once), guard(Not(c), c, line, "loop exit"), line));
    return gSeq(cs);
  }
  if (auto *o = dynamic_cast<ASTOutputStmt *>(s)) {
    std::vector<G> cs;
    I(o->getArg(), &cs, nullptr);
    return gSeq(cs);
  }
  if (dynamic_cast<ASTErrorStmt *>(s))
    return gSeq({assertion(FALSE, line, "error statement is unreachable"),
                 gAssume(FALSE, line, "the program stops")});
  throw Unsupported(where(s) + "unsupported statement in verification");
}

// A call g(args), against g's contract, not its body:
//   assert pre_g[args]; havoc r; assume post_g[args, r]
// and the call's value is r.  A recursive call must also decrease g's
// measure.  For each borrowed parameter g writes, the caller's variable x
// gets a new value x' that post_g describes.  `target` is the variable the
// value is assigned to, if any, so it can receive r directly.
T Lower::callExpr(ASTFunAppExpr *call, std::vector<G> &cs, const std::string &target) {
  int line = call->getLine();
  auto *fv = dynamic_cast<ASTVariableExpr *>(call->getFunction());
  auto *g = fns[fv->getName()];
  auto formals = g->getFormals();
  auto actuals = call->getActuals();
  std::vector<T> args;
  std::vector<std::string> roots;
  for (size_t k = 0; k < actuals.size(); ++k) {
    args.push_back(I(actuals[k], &cs, nullptr));
    roots.push_back(k < formals.size() && enc.isBorrow(formals[k]) ? name(rootOf(actuals[k])) : "");
  }
  Sub at, olds;
  for (size_t k = 0; k < formals.size() && k < args.size(); ++k)
    at[formals[k]->getName()] = olds[formals[k]->getName()] = args[k];

  if (edges) edges->insert({current->getName(), g->getName()});
  // A function with `decreases *` may not return, so its calls owe
  // nothing.  Any other function must return, so it may not call one
  // that might not: termination has to hold across calls, or a lemma
  // could borrow `ensures false` from a function that never returns.
  if (!current->mayDiverge() && g->mayDiverge()) {
    cs.push_back(missingClause(line, g->getName() + " may not terminate; " + current->getName() +
                                         " needs decreases *"));
  } else if (!current->mayDiverge() &&
             (g == current || (cycles && cycles->mutual(current->getName(), g->getName())))) {
    bool both = false;
    auto what = cycleObligation(current, g, both);
    if (both) {
      auto now = measure(g->getDecreases(), &at), before = measure(current->getDecreases(), &oldAll);
      cs.push_back(assertion(And({op("<=", {num(0), now}), op("<", {now, before})}), line, what));
    } else {
      cs.push_back(missingClause(line, what));
    }
  }
  // The call as written, with its arguments: a contract is instantiated
  // at each call, so its obligations are named by the call.
  std::vector<std::string> shown;
  for (auto &a : args) shown.push_back(flat(a));
  std::string site = g->getName() + "(" + join(shown) + ")";
  std::vector<T> req;
  for (auto *e : g->getRequires()) req.push_back(spec(e, &at));
  cs.push_back(assertion(And(req), line, "precondition of " + site));

  // The in-out model needs the written places to be distinct: two borrows
  // of one variable, either of them written, is the case it cannot handle.
  std::vector<std::string> outs, outVars;
  bool overlap = false;
  for (size_t k = 0; k < roots.size(); ++k) {
    if (!writes.isWritten(g->getName(), (int)k)) continue;
    if (roots[k].empty())
      throw Unsupported(where(call) + "a written borrow must name a variable");
    for (size_t j = 0; j < roots.size(); ++j)
      overlap = overlap || (j != k && roots[j] == roots[k]);
    if (std::count(outs.begin(), outs.end(), roots[k])) continue;
    auto after = roots[k] + "'";
    proc_.sorts[after] = proc_.sortOf(roots[k]);
    at[formals[k]->getName()] = var(after);
    outs.push_back(roots[k]);
    outVars.push_back(after);
  }

  if (overlap) cs.push_back(assertion(FALSE, line, "borrowed arguments do not overlap"));

  std::set<std::string> fv2;
  for (auto &a : args) freeVars(a, fv2);
  std::string r = target;
  if (r.empty() || fv2.count(r) || std::count(outs.begin(), outs.end(), r))
    r = g->getName() + "#" + std::to_string(fresh++);
  proc_.sorts[r] = enc.resultSort(g->getName());
  std::vector<std::string> hv{r};
  hv.insert(hv.end(), outVars.begin(), outVars.end());
  cs.push_back(havoc(hv, line, "result of " + site));
  at["result"] = var(r);
  std::vector<T> ens;
  for (auto *e : g->getEnsures()) ens.push_back(spec(e, &at, &olds));
  cs.push_back(tracked(gAssume(And(ens), line, "postcondition of " + site),
                       g->isLemma() ? Assumption::Lemma : Assumption::Contract,
                       (g->isLemma() ? "lemma " : "contract of ") + g->getName() + " (line " +
                           std::to_string(line) + ")",
                       g->getName()));
  for (size_t k = 0; k < outs.size(); ++k)
    cs.push_back(gAssign(outs[k], var(outVars[k]), line, "written through the borrow"));
  return var(r);
}

bool Lower::machineInts() const { return overflow && !ghost && !current->isLemma(); }

} // namespace verutop

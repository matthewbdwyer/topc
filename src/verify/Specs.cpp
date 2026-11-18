#include "Specs.h"

#include "Encoding.h"
#include "Support.h"
#include "TermPrint.h"
#include "VCGen.h"

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

namespace verutop {

SpecFns::SpecFns(ASTProgram *p, Lower &low) : low(low) {
  // Once to find the calls between spec functions, once knowing the cycles.
  CallEdges edges;
  low.onSpecCall = [&](ASTFunction *g, const std::vector<T> &, int) {
    if (!inMeasure) edges.insert({current->getName(), g->getName()});
  };
  build(p);
  Cycles cycles(edges);
  decls.clear();
  axioms.clear();
  lhs.clear();
  rhs.clear();
  checks.clear();
  skipped.clear();
  low.onSpecCall = [&](ASTFunction *g, const std::vector<T> &args, int line) {
    if (inMeasure) return;
    if (g == current || cycles.mutual(current->getName(), g->getName())) termination(g, args, line);
  };
  build(p);
  low.onSpecCall = nullptr;
}

void SpecFns::build(ASTProgram *p) {
  low.sortLookup = [this](const std::string &x) {
    auto it = sorts.find(x);
    return it == sorts.end() ? std::string("Int") : it->second;
  };
  for (auto *f : p->getFunctions()) {
    if (!f->isSpec()) continue;
    try {
      current = f;
      env.clear();
      path.clear();
      sorts.clear();
      std::string params, sig;
      std::vector<T> xs;
      for (auto *x : f->getFormals()) {
        auto s = low.enc.sortOf(x);
        sorts[x->getName()] = s;
        env[x->getName()] = var(x->getName());
        params += "(|" + x->getName() + "| " + s + ")";
        sig += " " + s;
        xs.push_back(var(x->getName()));
      }
      for (auto *d : f->getDeclarations())
        for (auto *v : d->getVars()) {
          auto s = low.enc.sortOf(v);
          sorts[v->getName()] = s;
          env[v->getName()] = s == "Int" ? num(0) : var("unset!" + f->getName() + "." + v->getName());
        }
      params0 = env;
      auto stmts = f->getStmts();
      for (size_t i = 0; i + 1 < stmts.size(); ++i) stmt(stmts[i]);
      auto *ret = dynamic_cast<ASTReturnStmt *>(stmts.back());
      T body = low.I(ret->getArg(), nullptr, &env);
      T call = app(f->getName(), xs);
      lhs[f->getName()] = call;
      rhs[f->getName()] = body;
      decls += "(declare-fun |" + f->getName() + "| (" + sig + ") " + low.enc.resultSort(f->getName()) + ")\n";
      // A local of a datatype sort that some path leaves unset.
      std::set<std::string> fv;
      freeVars(body, fv);
      for (auto &u : fv)
        if (u.rfind("unset!", 0) == 0)
          decls += "(declare-const |" + u + "| " + sorts[u.substr(u.rfind('.') + 1)] + ")\n";
      // The trigger tells Z3 to instantiate the axiom once for each
      // application it sees, which unfolds the definition exactly where a
      // proof needs it.
      if (xs.empty())
        axioms[f->getName()] = "(assert " + smt(Eq(call, body)) + ")\n";
      else
        axioms[f->getName()] = "(assert (forall (" + params + ") (! " + smt(Eq(call, body)) +
                               " :pattern (" + smt(call) + "))))\n";
    } catch (Unsupported &u) {
      skipped.push_back(f->getName() + ": " + u.what());
    }
  }
}

void SpecFns::stmt(ASTStmt *s) {
  if (dynamic_cast<ASTProofStmt *>(s))
    throw Unsupported(where(s) + "assert or assume in a spec function");
  if (dynamic_cast<ASTDestroyStmt *>(s) || dynamic_cast<ASTDeclStmt *>(s)) return;
  if (auto *b = dynamic_cast<ASTBlockStmt *>(s)) {
    for (auto *t : b->getStmts()) stmt(t);
    return;
  }
  if (auto *a = dynamic_cast<ASTAssignStmt *>(s)) {
    auto x = rootOf(a->getLHS());
    if (x.empty()) throw Unsupported(where(s) + "unsupported assignment target");
    env[x] = low.I(a->getRHS(), nullptr, &env);
    return;
  }
  if (auto *i = dynamic_cast<ASTIfStmt *>(s)) {
    auto c = low.B(i->getCondition(), nullptr, &env);
    Sub before = env;
    path.push_back(c);
    stmt(i->getThen());
    Sub e1 = env;
    env = before;
    path.back() = Not(c);
    if (i->getElse()) stmt(i->getElse());
    path.pop_back();
    Sub e2 = env;
    env = before;
    for (auto &[x, v0] : before)
      if (smt(e1[x]) != smt(e2[x])) env[x] = op("ite", {c, e1[x], e2[x]});
      else env[x] = e1[x];
    return;
  }
  if (auto *c = dynamic_cast<ASTCaseStmt *>(s)) {
    T t = low.I(c->getCaseExpr(), nullptr, &env);
    Sub before = env;
    std::vector<T> conds, earlier;
    std::vector<Sub> outs;
    for (auto *arm : c->getArms()) {
      std::vector<std::pair<ASTDeclNode *, T>> binds;
      T m = matchCtor(low.enc, arm->getTag(), arm->getPatterns(), t, binds);
      env = before;
      for (auto &[d, v] : binds) {
        env[d->getName()] = v;
        sorts[d->getName()] = low.enc.sortOfTerm(v, low.sortLookup);
      }
      auto here = earlier;
      here.push_back(m);
      path.push_back(And(here));
      stmt(arm->getBody());
      path.pop_back();
      outs.push_back(env);
      conds.push_back(m);
      earlier.push_back(Not(m));
    }
    Sub res = before;
    for (int k = (int)outs.size() - 1; k >= 0; --k)
      for (auto &[x, v0] : before)
        if (smt(outs[k][x]) != smt(res[x])) res[x] = op("ite", {conds[k], outs[k][x], res[x]});
    env = res;
    return;
  }
  throw Unsupported(where(s) + "a spec function may not contain this statement");
}

// On the path to a call of g on a cycle (g may be the function itself),
// g's measure at the call is smaller than this function's at entry, and
// not negative.
void SpecFns::termination(ASTFunction *g, const std::vector<T> &args, int line) {
  T P = FALSE;
  bool both = false;
  std::string what = cycleObligation(current, g, both);
  if (both) {
    Sub atCall = params0, atEntry = params0;
    auto formals = g->getFormals();
    for (size_t k = 0; k < formals.size() && k < args.size(); ++k)
      atCall[formals[k]->getName()] = args[k];
    inMeasure = true;
    auto now = low.measure(g->getDecreases(), &atCall);
    auto before = low.measure(current->getDecreases(), &atEntry);
    inMeasure = false;
    P = And({op("<=", {num(0), now}), op("<", {now, before})});
  }
  T F = path.empty() ? P : Implies(And(path), P);
  std::string q;
  for (auto *x : current->getFormals())
    q += "(declare-const |" + x->getName() + "| " + sorts[x->getName()] + ")\n";
  q += "(assert (not " + smt(F) + "))\n";
  checks.push_back({current->getName(), {line, what}, q, F});
}

namespace {
// A spec function exists only in specifications: code may not mention it.
// The visitor does not enter specifications, so only code is checked.
class SpecUse : public ASTVisitor {
  std::set<std::string> specs, lemmas;
  bool inSpecFn = false, inLemma = false;

public:
  explicit SpecUse(ASTProgram *p) {
    for (auto *f : p->getFunctions()) {
      if (f->isSpec()) specs.insert(f->getName());
      if (f->isLemma()) lemmas.insert(f->getName());
    }
  }
  bool visit(ASTFunction *f) override {
    inSpecFn = f->isSpec();
    inLemma = f->isLemma();
    // A spec function or a lemma that did not terminate would prove anything.
    if ((f->isSpec() || f->isLemma()) && f->mayDiverge())
      throw SemanticError(std::string(f->isSpec() ? "spec function " : "lemma ") + f->getName() +
                          " has decreases *; only code may fail to terminate");
    return true;
  }
  void endVisit(ASTWhileStmt *w) override {
    if (inLemma && w->mayDiverge())
      throw SemanticError("loop with decreases * in a lemma on line " +
                          std::to_string(w->getLine()) + "; a lemma must terminate");
  }
  void endVisit(ASTRankExpr *r) override {
    if (!inSpecFn)
      throw SemanticError("rank used in code on line " + std::to_string(r->getLine()) +
                        "; rank exists only in specifications");
  }
  void endVisit(ASTVariableExpr *v) override {
    if (!inSpecFn && specs.count(v->getName()))
      throw SemanticError("spec function " + v->getName() + " used in code on line " +
                          std::to_string(v->getLine()) +
                          "; spec functions are erased and exist only in specifications");
    if (lemmas.count(v->getName()))
      throw SemanticError("lemma " + v->getName() + " used on line " +
                          std::to_string(v->getLine()) +
                          "; a lemma is erased and is applied only with use");
  }
};
} // namespace

} // namespace verutop

void VCGen::checkSpecUse(ASTProgram *program) {
  if (program == nullptr) return;
  verutop::SpecUse check(program);
  program->accept(&check);
}

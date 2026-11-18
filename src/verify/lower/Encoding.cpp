#include "Encoding.h"

#include "Support.h"

#include <tuple>
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

namespace verutop {

std::string Encoding::sortOf(const std::shared_ptr<TopType> &t) const {
  if (!t || dynamic_cast<TopInt *>(t.get())) return "Int";
  if (auto *r = dynamic_cast<ReferenceType *>(t.get())) return sortOf(r->getReferencedType());
  if (auto *s = dynamic_cast<TopSumType *>(t.get())) return "|" + s->getTypeName() + "|";
  if (auto *m = dynamic_cast<TopMu *>(t.get())) return sortOf(m->getT());
  if (dynamic_cast<TopFunction *>(t.get())) throw Unsupported("function values are not supported");
  return "Int"; // an unconstrained type variable
}

bool Encoding::isBorrow(const std::shared_ptr<TopType> &t) {
  auto *r = dynamic_cast<ReferenceType *>(t.get());
  if (!r) return false;
  auto m = std::dynamic_pointer_cast<ReferenceMode>(r->getMode());
  return m && m->getMode() == ReferenceMode::Mode::Borrow;
}

void Encoding::build(ASTProgram *p, TypeInference *types) {
  ti = types;
  findWrites(p);
  std::string dts, ctors;
  for (auto *td : p->getTypedecls()) {
    std::string s = "|" + td->getName() + "|";
    dts += " (" + s + " 0)";
    ctors += " (";
    for (auto *v : td->getVariants()) {
      ctorSort[v->getTag()] = s;
      ctors += "(|" + v->getTag() + "|";
      for (auto *f : v->getParams()) {
        std::string sl = v->getTag() + "." + f->getName(), so = sortOf(f);
        fields[v->getTag()].push_back({sl, so});
        selSort[sl] = so;
        ctors += " (|" + sl + "| " + so + ")";
      }
      ctors += ")";
    }
    ctors += ")";
  }
  if (!dts.empty()) decls += "(declare-datatypes (" + dts + ") (" + ctors + "))\n";
  for (auto *f : p->getFunctions()) {
    auto ft = std::dynamic_pointer_cast<TopFunction>(ti->getInferredType(f->getDecl()));
    try {
      fnSort[f->getName()] = ft ? sortOf(ft->getReturnType()) : "Int";
    } catch (Unsupported &) {
      fnSort[f->getName()] = "Int";
    }
  }
  // rank#D measures the size of a D value, for `decreases` on a datatype:
  // rank(C(x1, ..., xn)) = 1 + the ranks of the fields that are datatypes.
  // That a rank is at least 1 is true but needs induction, so it is
  // stated as part of the (trusted) axiom.
  for (auto *td : p->getTypedecls()) {
    std::string s = "|" + td->getName() + "|", r = "rank#" + td->getName();
    decls += "(declare-fun |" + r + "| (" + s + ") Int)\n";
    T x = var("x");
    T body = num(1);
    auto vs = td->getVariants();
    for (int i = (int)vs.size() - 1; i >= 0; --i) {
      std::vector<T> sum{num(1)};
      for (auto &[sl, so] : fields[vs[i]->getTag()])
        if (isDatatype(so)) sum.push_back(rank(sel(sl, x), so));
      T e = sum[0];
      for (size_t k = 1; k < sum.size(); ++k) e = op("+", {e, sum[k]});
      body = i + 1 == (int)vs.size() ? e : op("ite", {is(vs[i]->getTag(), x), e, body});
    }
    // The axiom also says rank >= 1, which needs induction.  This query
    // is the induction step: if every sum-valued field has rank >= 1, the
    // value does.  Structural induction then gives it for all values.
    // It uses no definitions: it is what justifies one.
    Query step;
    step.consts = {{"x", s}};
    for (auto *v : vs)
      for (auto &[sl, so] : fields[v->getTag()])
        if (isDatatype(so))
          step.hypotheses.push_back(Implies(is(v->getTag(), x), op(">=", {rank(sel(sl, x), so), num(1)})));
    step.goal = op(">=", {body, num(1)});
    step.definitions = false;
    rankChecks[r] = step;
    T call = app(r, {x});
    rankAxioms[r] = Axiom{r, {{"x", s}}, call, op("and", {op(">=", {call, num(1)}), Eq(call, body)})};
  }
}

T Encoding::rank(T t, const std::string &sort) {
  return app("rank#" + sort.substr(1, sort.size() - 2), {std::move(t)});
}

// The sort of a term; `lookup` gives the sort of a variable.
std::string Encoding::resultSort(const std::string &fn) const {
  auto it = fnSort.find(fn);
  return it == fnSort.end() ? "" : it->second;
}

const std::vector<std::pair<std::string, std::string>> &Encoding::fieldsOf(const std::string &tag) const {
  static const std::vector<std::pair<std::string, std::string>> none;
  auto it = fields.find(tag);
  return it == fields.end() ? none : it->second;
}

std::string Encoding::sortOfTerm(const T &t, const std::function<std::string(const std::string &)> &lookup) const {
  switch (t->kind) {
  case Term::Num: return "Int";
  case Term::Var: return lookup(t->name);
  case Term::Fn: {
    auto it = fnSort.find(t->name);
    return it == fnSort.end() ? "Int" : it->second;
  }
  case Term::Ctor: return ctorSort.count(t->name) ? ctorSort.at(t->name) : "Int";
  case Term::Sel: return selSort.count(t->name) ? selSort.at(t->name) : "Int";
  case Term::Is:
  case Term::Forall: return "Bool";
  case Term::Let: return sortOfTerm(t->args[1], lookup);
  case Term::Op:
    if (t->name == "ite") return sortOfTerm(t->args[1], lookup);
    if (t->name == "+" || t->name == "-" || t->name == "*" || t->name == "tdiv") return "Int";
    return "Bool";
  }
  return "Int";
}

// ---- references -------------------------------------------------------------

// The variable an lvalue or a borrowed argument names: x, *x, **x, &x.
std::string Encoding::rootOf(ASTExpr *e, int *derefs) {
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

// ---- borrows -----------------------------------------------------------------

void Encoding::findWrites(ASTProgram *p) {
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
        if (!isBorrow(formals[k]) || written.count({f->getName(), (int)k})) continue;
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
std::set<std::string> Encoding::assigned(ASTNode *s) const {
  struct Scan : ASTVisitor {
    const Encoding *w;
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

// ---- sum values ----------------------------------------------------------------

T Encoding::matchCtor(const std::string &tag, const std::vector<ASTPattern *> &subs, const T &t,
                      std::vector<std::pair<ASTDeclNode *, T>> &binds) const {
  std::vector<T> cs{is(tag, t)};
  auto &fs = fieldsOf(tag);
  for (size_t i = 0; i < subs.size() && i < fs.size(); ++i)
    cs.push_back(matchPattern(subs[i], sel(fs[i].first, t), binds));
  return And(cs);
}
T Encoding::matchPattern(ASTPattern *p, const T &t, std::vector<std::pair<ASTDeclNode *, T>> &binds) const {
  if (auto *c = dynamic_cast<ASTCtorPattern *>(p))
    return matchCtor(c->getTag(), c->getSubPatterns(), t, binds);
  if (auto *v = dynamic_cast<ASTVarPattern *>(p)) binds.push_back({v->getDecl(), t});
  return TRUE;
}

// ---- machine integers --------------------------------------------------------

T Encoding::inRange(const T &t) { return op("and", {op("<=", {op("INT_MIN"), t}), op("<=", {t, op("INT_MAX")})}); }

} // namespace verutop

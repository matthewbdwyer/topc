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

namespace verutop {

std::string Types::sortOf(const std::shared_ptr<TopType> &t) const {
  if (!t || dynamic_cast<TopInt *>(t.get())) return "Int";
  if (auto *r = dynamic_cast<ReferenceType *>(t.get())) return sortOf(r->getReferencedType());
  if (auto *s = dynamic_cast<TopSumType *>(t.get())) return "|" + s->getTypeName() + "|";
  if (auto *m = dynamic_cast<TopMu *>(t.get())) return sortOf(m->getT());
  if (dynamic_cast<TopFunction *>(t.get())) throw Unsupported("function values are not supported");
  return "Int"; // an unconstrained type variable
}

bool Types::isBorrow(const std::shared_ptr<TopType> &t) {
  auto *r = dynamic_cast<ReferenceType *>(t.get());
  if (!r) return false;
  auto m = std::dynamic_pointer_cast<ReferenceMode>(r->getMode());
  return m && m->getMode() == ReferenceMode::Mode::Borrow;
}

void Types::build(ASTProgram *p, TypeInference *types) {
  ti = types;
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
    std::string step = "(declare-const |x| " + s + ")\n";
    for (auto *v : vs)
      for (auto &[sl, so] : fields[v->getTag()])
        if (isDatatype(so))
          step += "(assert " + smt(Implies(is(v->getTag(), x), op(">=", {rank(sel(sl, x), so), num(1)}))) + ")\n";
    rankChecks[r] = step + "(assert (not " + smt(op(">=", {body, num(1)})) + "))\n";
    T call = app(r, {x});
    rankAxioms[r] = "(assert (forall ((|x| " + s + ")) (! (and (>= " + smt(call) + " 1) " +
                    smt(Eq(call, body)) + ") :pattern (" + smt(call) + "))))\n";
  }
}

T Types::rank(T t, const std::string &sort) {
  return app("rank#" + sort.substr(1, sort.size() - 2), {std::move(t)});
}

Types TY;

// The sort of a term; `lookup` gives the sort of a variable.
std::string sortOfTerm(const T &t, const std::function<std::string(const std::string &)> &lookup) {
  switch (t->kind) {
  case Term::Num: return "Int";
  case Term::Var: return lookup(t->name);
  case Term::Fn: {
    auto it = TY.fnSort.find(t->name);
    return it == TY.fnSort.end() ? "Int" : it->second;
  }
  case Term::Ctor: return TY.ctorSort.count(t->name) ? TY.ctorSort[t->name] : "Int";
  case Term::Sel: return TY.selSort.count(t->name) ? TY.selSort[t->name] : "Int";
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

} // namespace verutop

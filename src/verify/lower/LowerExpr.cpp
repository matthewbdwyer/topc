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

// Expressions.  See Lowering.h for what `pre` and `sub` mean.

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
    auto s = sortOf(t);
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
        pre->push_back(assertion(Encoding::inRange(t), e, ObKind::NoOverflow, flat(t)));
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
        pre->push_back(assertion(Not(Eq(t->args[1], num(0))), e, ObKind::DivisorNotZero));
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
    scope->specCall(g, as, call);
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

} // namespace verutop

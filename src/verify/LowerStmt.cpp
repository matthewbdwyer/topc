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

// Statements.

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
        if (enc.isWritten(fv->getName(), (int)k))
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
    return assertion(P, s, ObKind::Assertion);
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
    auto root = Encoding::rootOf(a->getLHS());
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
      T m = enc.matchCtor(arm->getTag(), arm->getPatterns(), t, binds);
      auto saved = renames;
      std::vector<G> body{guard(m, m, arm->getLine(), "case " + showArm(arm))};
      for (auto &[d, v] : binds) {
        auto b = d->getName();
        auto bs = sortOf(v);
        std::string target = b;
        if (proc_.sorts.count(b) && proc_.sorts[b] != bs) target = b + "#" + std::to_string(fresh++);
        proc_.sorts[target] = bs;
        renames[b] = target;
        body.push_back(gAssign(target, v, arm->getLine(), "bind " + b));
        if (machineInts() && bs == "Int")
          body.push_back(gAssume(Encoding::inRange(var(target)), arm->getLine(), "a 64-bit value"));
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
    std::vector<G> cs{assertion(J, s, ObKind::InvariantOnEntry)};
    // Every loop has a measure, or says with `decreases *` that it may
    // run forever.
    if (!w->getDecreases() && !w->mayDiverge())
      cs.push_back(assertion(FALSE, s, ObKind::MissingLoopMeasure));
    if (w->mayDiverge()) proc_.diverges.push_back("the loop at line " + std::to_string(line));
    if (w->mayDiverge() && !current->mayDiverge())
      cs.push_back(assertion(FALSE, s, ObKind::LoopMayNotTerminate, current->getName()));
    std::vector<std::string> mods;
    for (auto &x : enc.assigned(w->getBody())) mods.push_back(name(x));
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
    once.push_back(assertion(J, s, ObKind::InvariantPreserved));
    if (auto *D = w->getDecreases())
      once.push_back(assertion(And({op("<=", {num(0), var(m)}), op("<", {measure(D), var(m)})}), s,
                             ObKind::LoopMeasure));
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
    return gSeq({assertion(FALSE, s, ObKind::ErrorUnreachable),
                 gAssume(FALSE, line, "the program stops")});
  throw Unsupported(where(s) + "unsupported statement in verification");
}

} // namespace verutop

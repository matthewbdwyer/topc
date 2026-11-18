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

// Calls.

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
    roots.push_back(k < formals.size() && enc.isBorrow(formals[k]) ? name(Encoding::rootOf(actuals[k])) : "");
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
    cs.push_back(assertion(FALSE, call, ObKind::CallMayNotTerminate, current->getName(), g->getName()));
  } else if (!current->mayDiverge() &&
             (g == current || (cycles && cycles->mutual(current->getName(), g->getName())))) {
    Obligation ob = cycleObligation(current, g, call);
    T smaller = FALSE;
    if (!ob.missing()) {
      auto now = measure(g->getDecreases(), &at), before = measure(current->getDecreases(), &oldAll);
      smaller = And({op("<=", {num(0), now}), op("<", {now, before})});
    }
    cs.push_back(assertion(smaller, ob));
  }
  // The call as written, with its arguments: a contract is instantiated
  // at each call, so its obligations are named by the call.
  std::vector<std::string> shown;
  for (auto &a : args) shown.push_back(flat(a));
  std::string site = g->getName() + "(" + join(shown) + ")";
  std::vector<T> req;
  for (auto *e : g->getRequires()) req.push_back(spec(e, &at));
  cs.push_back(assertion(And(req), call, ObKind::Precondition, site, g->getName()));

  // The in-out model needs the written places to be distinct: two borrows
  // of one variable, either of them written, is the case it cannot handle.
  std::vector<std::string> outs, outVars;
  bool overlap = false;
  for (size_t k = 0; k < roots.size(); ++k) {
    if (!enc.isWritten(g->getName(), (int)k)) continue;
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

  if (overlap) cs.push_back(assertion(FALSE, call, ObKind::BorrowsDisjoint));

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

} // namespace verutop

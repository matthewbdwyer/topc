#include "BorrowChecker.h"

#include "ASTAssignStmt.h"
#include "ASTBinaryExpr.h"
#include "ASTBorrowExpr.h"
#include "ASTErrorStmt.h"
#include "ASTFunction.h"
#include "ASTFunAppExpr.h"
#include "ASTIfStmt.h"
#include "ASTOutputStmt.h"
#include "ASTProgram.h"
#include "ASTReturnStmt.h"
#include "ASTVariableExpr.h"
#include "ASTWhileStmt.h"
#include "FunctionEffectSummaries.h"
#include "SemanticError.h"
#include "../SemanticLogging.h"
#include "SymbolTable.h"

#include <algorithm>
#include <sstream>
#include <tuple>
#include <utility>

#include "loguru.hpp"

std::vector<BorrowChecker::BorrowTraceEvent> BorrowChecker::lastTrace;
std::vector<BorrowChecker::BorrowProvenanceEvent>
  BorrowChecker::lastProvenance;

bool BorrowChecker::visit(ASTFunction *element) {
  currentFuncDecl = element->getDecl();
  return true;
}

// ---------------------------------------------------------------------------
// visit(ASTFunAppExpr): mark every direct ASTBorrowExpr actual as approved.
// This runs before the children are visited (pre-order hook), so the borrow
// nodes will already be in the approved set when endVisit(ASTBorrowExpr) fires.
// ---------------------------------------------------------------------------
bool BorrowChecker::visit(ASTFunAppExpr *element) {
  auto *calleeVar = dynamic_cast<ASTVariableExpr *>(element->getFunction());
  std::ostringstream calleeRepr;
  calleeRepr << *element->getFunction();
  const std::string callee =
      calleeVar != nullptr ? calleeVar->getName() : calleeRepr.str();

  auto actuals = element->getActuals();
  for (std::size_t index = 0; index < actuals.size(); ++index) {
    auto *actual = actuals[index];
    auto *borrow = dynamic_cast<ASTBorrowExpr *>(actual);
    if (borrow != nullptr) {
      approvedBorrows.insert(borrow);
      std::ostringstream originRepr;
      originRepr << *borrow;
      addProvenance({BorrowProvenanceEvent::Kind::Direct,
                     borrow->getLine(),
                     borrow->getColumn(),
                     originRepr.str(),
                     0,
                     element->getLine(),
                     element->getColumn(),
                     originRepr.str(),
                     callee,
                     index});
      continue;
    }

    const auto origin = borrowOrigin(actual);
    if (origin.derived && origin.hasConcreteOrigin) {
      std::ostringstream actualRepr;
      actualRepr << *actual;
      addProvenance({BorrowProvenanceEvent::Kind::Flow,
                     origin.line,
                     origin.column,
                     origin.expr,
                     origin.hop,
                     element->getLine(),
                     element->getColumn(),
                     actualRepr.str(),
                     callee,
                     index});
    }
  }
  return true; // continue visiting children
}

// ---------------------------------------------------------------------------
// endVisit(ASTBorrowExpr): if this borrow was not pre-approved it is in an
// illegal position.
// ---------------------------------------------------------------------------
void BorrowChecker::endVisit(ASTBorrowExpr *element) {
  std::ostringstream repr;
  repr << *element;

  if (approvedBorrows.find(element) == approvedBorrows.end()) {
    trace.push_back(
        {element->getLine(), element->getColumn(), repr.str(), false});
    lastTrace = trace;
    std::ostringstream oss;
    oss << "Borrow error on line " << element->getLine()
        << ": borrow expression must be an immediate function argument"
           " — storing a borrow in a variable or using it in any other"
           " position is not permitted";
    throw SemanticError(oss.str());
  }

  trace.push_back({element->getLine(), element->getColumn(), repr.str(), true});
}

void BorrowChecker::endVisit(ASTAssignStmt *element) {
  if (!checkCallReturns) {
    return;
  }
  if (exprReturnsBorrow(element->getRHS())) {
    rejectBorrowEscape(element->getRHS(), "assignment");
  } // LCOV_EXCL_LINE -- unreachable brace: rejectBorrowEscape always throws
}

void BorrowChecker::endVisit(ASTBinaryExpr *element) {
  if (!checkCallReturns) {
    return;
  }
  if (exprReturnsBorrow(element->getLeft()) ||
      exprReturnsBorrow(element->getRight())) {
    // LCOV_EXCL_START -- unreachable: a binary operand must be int, so type
    // inference rejects a borrow-returning call here before borrow checking.
    rejectBorrowEscape(element, "arithmetic or relational expression");
  }
  // LCOV_EXCL_STOP
}

void BorrowChecker::endVisit(ASTOutputStmt *element) {
  if (!checkCallReturns) {
    return;
  }
  if (exprReturnsBorrow(element->getArg())) {
    // LCOV_EXCL_START -- unreachable: `output` requires an int argument, so type
    // inference rejects a borrow-returning call here before borrow checking.
    rejectBorrowEscape(element->getArg(), "output");
  }
  // LCOV_EXCL_STOP
}

void BorrowChecker::endVisit(ASTErrorStmt *element) {
  if (!checkCallReturns) {
    return;
  }
  if (exprReturnsBorrow(element->getArg())) {
    // LCOV_EXCL_START -- unreachable: `error` requires an int argument, so type
    // inference rejects a borrow-returning call here before borrow checking.
    rejectBorrowEscape(element->getArg(), "error");
  }
  // LCOV_EXCL_STOP
}

void BorrowChecker::endVisit(ASTReturnStmt *element) {
  if (!checkCallReturns) {
    return;
  }
  if (exprReturnsBorrow(element->getArg())) {
    rejectBorrowEscape(element->getArg(), "return");
  } // LCOV_EXCL_LINE -- unreachable brace: rejectBorrowEscape always throws
}

void BorrowChecker::endVisit(ASTIfStmt *element) {
  if (!checkCallReturns) {
    return;
  }
  if (exprReturnsBorrow(element->getCondition())) {
    // LCOV_EXCL_START -- unreachable: an `if` condition must be int, so type
    // inference rejects a borrow-returning call here before borrow checking.
    rejectBorrowEscape(element->getCondition(), "if condition");
  }
  // LCOV_EXCL_STOP
}

void BorrowChecker::endVisit(ASTWhileStmt *element) {
  if (!checkCallReturns) {
    return;
  }
  if (exprReturnsBorrow(element->getCondition())) {
    // LCOV_EXCL_START -- unreachable: a `while` condition must be int, so type
    // inference rejects a borrow-returning call here before borrow checking.
    rejectBorrowEscape(element->getCondition(), "while condition");
  }
  // LCOV_EXCL_STOP
}

BorrowChecker::BorrowOrigin BorrowChecker::borrowOrigin(ASTExpr *expr) const {
  if (expr == nullptr) {
    return {}; // LCOV_EXCL_LINE -- defensive: callers pass concrete expressions
  }

  if (auto *borrow = dynamic_cast<ASTBorrowExpr *>(expr)) {
    std::ostringstream repr;
    repr << *borrow;
    return {true, true, borrow->getLine(), borrow->getColumn(), repr.str(), 0};
  }

  auto *call = dynamic_cast<ASTFunAppExpr *>(expr);
  if (call == nullptr || sym == nullptr || functionEffects == nullptr) {
    return {};
  }

  auto *calleeVar = dynamic_cast<ASTVariableExpr *>(call->getFunction());
  ASTDeclNode *calleeDecl =
      (calleeVar != nullptr) ? sym->getFunction(calleeVar->getName()) : nullptr;
  const FunctionEffectSummaries::Summary *summary =
      (calleeDecl != nullptr) ? functionEffects->get(calleeDecl) : nullptr;
  if (summary == nullptr) {
    return {};
  }

  switch (summary->returnOrigin) {
  // LCOV_EXCL_START -- unreachable in TOP v1: a summary is BorrowFromFormal only
  // when a function returns a borrow derived from a formal (e.g. `q = &p;
  // return q;`). Producing such a summary requires storing or returning a
  // borrow, both of which the borrow checker rejects, so no surviving program
  // reaches a caller that would trace provenance through this arm. Retained for
  // when returning borrows becomes supported.
  case FunctionEffectSummaries::ReturnOrigin::BorrowFromFormal: {
    int index = summary->returnFormalIndex;
    auto actuals = call->getActuals();
    if (index >= 0 && static_cast<std::size_t>(index) < actuals.size()) {
      auto origin = borrowOrigin(actuals[static_cast<std::size_t>(index)]);
      if (origin.derived) {
        origin.hop++;
        return origin;
      }
    }
    return {true, false, 0, 0, "", 0};
  }
  // LCOV_EXCL_STOP
  case FunctionEffectSummaries::ReturnOrigin::FromFormal: {
    int index = summary->returnFormalIndex;
    auto actuals = call->getActuals();
    if (index < 0 || static_cast<std::size_t>(index) >= actuals.size()) {
      return {}; // LCOV_EXCL_LINE -- defensive: formal index is always in range
    }
    auto origin = borrowOrigin(actuals[static_cast<std::size_t>(index)]);
    if (origin.derived) {
      origin.hop++;
    }
    return origin;
  }
  case FunctionEffectSummaries::ReturnOrigin::Unknown:
  case FunctionEffectSummaries::ReturnOrigin::PureCopy:
  case FunctionEffectSummaries::ReturnOrigin::FreshOwn:
    return {};
  }

  return {}; // LCOV_EXCL_LINE -- unreachable: the switch above is exhaustive
}

bool BorrowChecker::exprReturnsBorrow(ASTExpr *expr) const {
  return borrowOrigin(expr).derived;
}

void BorrowChecker::addProvenance(BorrowProvenanceEvent event) {
  for (const auto &existing : provenance) {
    if (existing.kind == event.kind && existing.originLine == event.originLine &&
        existing.originColumn == event.originColumn &&
        existing.hop == event.hop && existing.useLine == event.useLine &&
        existing.useColumn == event.useColumn &&
        existing.argumentIndex == event.argumentIndex &&
        existing.expression == event.expression &&
        existing.callee == event.callee) {
      return; // LCOV_EXCL_LINE -- defensive: one pass produces no exact-duplicate event
    }
  }
  provenance.push_back(std::move(event));
}

void BorrowChecker::publishResults() {
  std::sort(provenance.begin(), provenance.end(), [](const auto &left,
                                                     const auto &right) {
    return std::tie(left.originLine, left.originColumn, left.hop, left.useLine,
                    left.useColumn, left.argumentIndex, left.callee) <
           std::tie(right.originLine, right.originColumn, right.hop,
                    right.useLine, right.useColumn, right.argumentIndex,
                    right.callee);
  });
  lastTrace = trace;
  lastProvenance = provenance;
}

void BorrowChecker::rejectBorrowEscape(ASTExpr *expr,
                                       const std::string &sink) const {
  std::ostringstream repr;
  repr << *expr;

  std::ostringstream oss;
  oss << "Borrow error on line " << expr->getLine()
      << ": borrow-derived value " << repr.str() << " escapes into " << sink
      << "; a borrowed alias may only flow through immediate call arguments. "
         "Return or store a copy instead.";
  throw SemanticError(oss.str());
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------
void BorrowChecker::check(ASTProgram *p) {
  SEMANTIC_LOG(1, "borrow-direct") << "start";
  BorrowChecker checker;
  p->accept(&checker);
  checker.publishResults();
  SEMANTIC_LOG(1, "borrow-direct")
      << "complete events=" << checker.trace.size();
}

void BorrowChecker::checkInterprocedural(ASTProgram *p, SymbolTable *sym,
                                         FunctionEffectSummaries *effects) {
  SEMANTIC_LOG(1, "borrow-interprocedural") << "start";
  BorrowChecker checker(sym, effects);
  p->accept(&checker);
  checker.publishResults();
  SEMANTIC_LOG(1, "borrow-interprocedural")
      << "complete events=" << checker.trace.size();
}

const std::vector<BorrowChecker::BorrowTraceEvent> &
BorrowChecker::getLastTrace() {
  return lastTrace;
}

const std::vector<BorrowChecker::BorrowProvenanceEvent> &
BorrowChecker::getLastProvenance() {
  return lastProvenance;
}

#include "BorrowChecker.h"

#include "ASTBorrowExpr.h"
#include "ASTFunAppExpr.h"
#include "ASTProgram.h"
#include "SemanticError.h"

#include <sstream>

#include "loguru.hpp"

std::vector<BorrowChecker::BorrowTraceEvent> BorrowChecker::lastTrace;

// ---------------------------------------------------------------------------
// visit(ASTFunAppExpr): mark every direct ASTBorrowExpr actual as approved.
// This runs before the children are visited (pre-order hook), so the borrow
// nodes will already be in the approved set when endVisit(ASTBorrowExpr) fires.
// ---------------------------------------------------------------------------
bool BorrowChecker::visit(ASTFunAppExpr *element) {
  for (auto *actual : element->getActuals()) {
    auto *borrow = dynamic_cast<ASTBorrowExpr *>(actual);
    if (borrow != nullptr) {
      approvedBorrows.insert(borrow);
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

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------
void BorrowChecker::check(ASTProgram *p) {
  LOG_S(1) << "Checking borrow lifetime validity";
  BorrowChecker checker;
  p->accept(&checker);
  lastTrace = checker.trace;
}

const std::vector<BorrowChecker::BorrowTraceEvent> &
BorrowChecker::getLastTrace() {
  return lastTrace;
}

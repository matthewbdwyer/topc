#include "CheckBorrowPositions.h"
#include "PrettyPrinter.h"
#include "SemanticError.h"

#include <sstream>

#include "loguru.hpp"

static bool isBorrow(ASTExpr *e) {
  return dynamic_cast<ASTBorrowExpr *>(e) != nullptr;
}

void CheckBorrowPositions::endVisit(ASTBinaryExpr *element) {
  if (isBorrow(element->getLeft()) || isBorrow(element->getRight())) {
    std::ostringstream oss;
    oss << "Borrow error on line " << element->getLine()
        << ": borrow expression cannot be used in arithmetic or relational "
           "expression\n";
    throw SemanticError(oss.str());
  }
}

void CheckBorrowPositions::endVisit(ASTOutputStmt *element) {
  if (isBorrow(element->getArg())) {
    std::ostringstream oss;
    oss << "Borrow error on line " << element->getLine()
        << ": borrow expression cannot be the argument of 'output'\n";
    throw SemanticError(oss.str());
  }
}

void CheckBorrowPositions::endVisit(ASTErrorStmt *element) {
  if (isBorrow(element->getArg())) {
    std::ostringstream oss;
    oss << "Borrow error on line " << element->getLine()
        << ": borrow expression cannot be the argument of 'error'\n";
    throw SemanticError(oss.str());
  }
}

void CheckBorrowPositions::endVisit(ASTReturnStmt *element) {
  if (isBorrow(element->getArg())) {
    std::ostringstream oss;
    oss << "Borrow error on line " << element->getLine()
        << ": borrow expression cannot appear in a 'return' statement\n";
    throw SemanticError(oss.str());
  }
}

void CheckBorrowPositions::check(ASTProgram *p) {
  LOG_S(1) << "Checking borrow positions";
  CheckBorrowPositions visitor;
  p->accept(&visitor);
}

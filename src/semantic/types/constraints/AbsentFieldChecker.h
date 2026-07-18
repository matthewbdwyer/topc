#pragma once

#include "ASTVisitor.h"
#include "Unifier.h"

/*! \brief Visits AST and checks that all field accesses are to defined fields. */
class AbsentFieldChecker : public ASTVisitor {
  Unifier *unifier;

public:
  explicit AbsentFieldChecker(Unifier *u) : unifier(u) {}

  static void check(ASTProgram *p, Unifier *u);

  void endVisit(ASTFieldAccessExpr *element) override;
};
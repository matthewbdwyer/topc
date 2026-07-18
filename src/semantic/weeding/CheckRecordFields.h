#pragma once

#include "ASTVisitor.h"

/*! \class CheckRecordFields
 *  \brief Reject record literals with duplicate field names.
 */
class CheckRecordFields : public ASTVisitor {
public:
  CheckRecordFields() = default;
  static void check(ASTProgram *p);
  virtual void endVisit(ASTRecordExpr *element) override;
};

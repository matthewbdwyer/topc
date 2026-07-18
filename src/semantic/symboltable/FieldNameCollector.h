#pragma once

#include "ASTVisitor.h"

#include <string>
#include <vector>

/*! \brief Collects all field names referenced within the program. */
class FieldNameCollector : public ASTVisitor {
  std::vector<std::string> fields;

public:
  FieldNameCollector() = default;

  static std::vector<std::string> build(ASTProgram *p);
  void endVisit(ASTRecordExpr *element) override;
  void endVisit(ASTFieldAccessExpr *element) override;
};
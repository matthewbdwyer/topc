#include "FieldNameCollector.h"

#include <algorithm>

namespace {
void addUnique(std::vector<std::string> &fields, const std::string &field) {
  if (std::find(fields.begin(), fields.end(), field) == fields.end()) {
    fields.push_back(field);
  }
}
} // namespace

void FieldNameCollector::endVisit(ASTRecordExpr *element) {
  for (const auto &field : element->getFieldNames()) {
    addUnique(fields, field);
  }
}

void FieldNameCollector::endVisit(ASTFieldAccessExpr *element) {
  addUnique(fields, element->getField());
}

std::vector<std::string> FieldNameCollector::build(ASTProgram *p) {
  FieldNameCollector visitor;
  p->accept(&visitor);
  return visitor.fields;
}
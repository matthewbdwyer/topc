#include "ASTRecordExpr.h"
#include "ASTVisitor.h"

std::vector<std::string> ASTRecordExpr::getFieldNames() const {
  std::vector<std::string> names;
  names.reserve(FIELDS.size());
  for (const auto &f : FIELDS) {
    names.push_back(f.first);
  }
  return names;
}

std::vector<ASTExpr *> ASTRecordExpr::getFieldValues() const {
  std::vector<ASTExpr *> values;
  values.reserve(FIELDS.size());
  for (const auto &f : FIELDS) {
    values.push_back(f.second.get());
  }
  return values;
}

void ASTRecordExpr::accept(ASTVisitor *visitor) {
  if (visitor->visit(this)) {
    for (auto *v : getFieldValues()) {
      v->accept(visitor);
    }
  }
  visitor->endVisit(this);
}

std::ostream &ASTRecordExpr::print(std::ostream &out) const {
  out << "{";
  bool first = true;
  for (std::size_t i = 0; i < FIELDS.size(); i++) {
    if (!first)
      out << ", ";
    out << FIELDS[i].first << ":" << *FIELDS[i].second;
    first = false;
  }
  out << "}";
  return out;
}

std::vector<std::shared_ptr<ASTNode>> ASTRecordExpr::getChildren() {
  std::vector<std::shared_ptr<ASTNode>> children;
  children.reserve(FIELDS.size());
  for (auto &f : FIELDS) {
    children.push_back(f.second);
  }
  return children;
}

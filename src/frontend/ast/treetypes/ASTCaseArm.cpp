#include "ASTCaseArm.h"
#include "ASTCtorPattern.h"
#include "ASTVarPattern.h"
#include "ASTVisitor.h"
#include "ASTinternal.h"

// Recursively collect DeclNodes from variable sub-patterns.
void ASTCaseArm::collectBindings(
    ASTPattern *p, std::vector<std::shared_ptr<ASTDeclNode>> &out) {
  if (auto *vp = dynamic_cast<ASTVarPattern *>(p)) {
    out.push_back(vp->getDeclShared());
  } else if (auto *cp = dynamic_cast<ASTCtorPattern *>(p)) {
    for (auto &sub : cp->getSubPatternsShared())
      collectBindings(sub.get(), out);
  }
  // ASTWildcardPattern: no bindings to collect
}

ASTCaseArm::ASTCaseArm(std::string tag,
                       std::vector<std::shared_ptr<ASTPattern>> patterns,
                       std::shared_ptr<ASTStmt> body)
    : TAG(std::move(tag)), PATTERNS(std::move(patterns)),
      BODY(std::move(body)) {
  for (auto &p : PATTERNS)
    collectBindings(p.get(), BINDINGS);
}

void ASTCaseArm::accept(ASTVisitor *visitor) {
  if (visitor->visit(this)) {
    // Visit flat bindings for backward compatibility with LocalNameCollector
    // and other visitors that expect DeclNode visits per arm binding.
    for (auto b : getBindings()) {
      b->accept(visitor);
    }
    getBody()->accept(visitor);
  }
  visitor->endVisit(this);
}

std::ostream &ASTCaseArm::print(std::ostream &out) const {
  out << TAG;
  if (!PATTERNS.empty()) {
    out << "(";
    bool first = true;
    for (auto &p : PATTERNS) {
      if (!first)
        out << ", ";
      out << *p;
      first = false;
    }
    out << ")";
  }
  out << " -> " << *BODY;
  return out;
}

std::vector<std::shared_ptr<ASTNode>> ASTCaseArm::getChildren() {
  std::vector<std::shared_ptr<ASTNode>> children;
  for (auto &b : BINDINGS)
    children.push_back(b);
  children.push_back(BODY);
  return children;
}

std::vector<ASTPattern *> ASTCaseArm::getPatterns() const {
  std::vector<ASTPattern *> r;
  for (auto &p : PATTERNS)
    r.push_back(p.get());
  return r;
}

std::vector<ASTDeclNode *> ASTCaseArm::getBindings() const {
  return rawRefs(BINDINGS);
}


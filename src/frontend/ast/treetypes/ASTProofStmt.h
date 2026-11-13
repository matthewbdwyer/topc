#pragma once

#include "ASTBlockStmt.h"
#include "ASTExpr.h"
#include "ASTVisitor.h"

/*! \brief VERUTOP: an assert, assume, or use (of a lemma) statement.
 *
 * To every pass that does not walk specifications it is an empty block, so
 * ownership, the CFG, and code generation need no change: the statement is
 * erased.  The symbol and type passes see its expression.
 */
class ASTProofStmt : public ASTBlockStmt {
public:
  enum class Kind { Assert, Assume, Use };

private:
  Kind KIND;
  std::shared_ptr<ASTExpr> EXPR;

public:
  ASTProofStmt(Kind kind, std::shared_ptr<ASTExpr> expr)
      : ASTBlockStmt({}), KIND(kind), EXPR(std::move(expr)) {}
  Kind getKind() const { return KIND; }
  ASTExpr *getExpr() const { return EXPR.get(); }

  void accept(ASTVisitor *visitor) override {
    if (visitor->visit(this) && visitor->visitSpecs()) {
      EXPR->accept(visitor);
    }
    visitor->endVisit(this);
  }

protected:
  std::ostream &print(std::ostream &out) const override {
    out << (KIND == Kind::Assert ? "assert " : KIND == Kind::Assume ? "assume " : "use ")
        << *EXPR << ";";
    return out;
  }
};

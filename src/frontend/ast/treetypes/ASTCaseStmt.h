#pragma once

#include "ASTCaseArm.h"
#include "ASTExpr.h"
#include "ASTStmt.h"

#include <vector>

/*! \brief Class for a case statement (pattern-match on a sum type).
 *
 * Example:
 *   case p of {
 *     Some(v) -> output v;
 *     None    -> output 0;
 *   }
 */
class ASTCaseStmt : public ASTStmt {
  std::shared_ptr<ASTExpr> SCRUTINEE;
  std::vector<std::shared_ptr<ASTCaseArm>> ARMS;

public:
  std::vector<std::shared_ptr<ASTNode>> getChildren() override;
  ASTCaseStmt(std::shared_ptr<ASTExpr> scrutinee,
              std::vector<std::shared_ptr<ASTCaseArm>> arms)
      : SCRUTINEE(std::move(scrutinee)), ARMS(std::move(arms)) {}
  ASTExpr *getScrutinee() const { return SCRUTINEE.get(); }
  std::vector<ASTCaseArm *> getArms() const;
  void accept(ASTVisitor *visitor) override;

protected:
  std::ostream &print(std::ostream &out) const override;
};

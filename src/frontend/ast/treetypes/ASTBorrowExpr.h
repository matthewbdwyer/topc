#pragma once

#include "ASTExpr.h"

/*! \brief Class for a borrow expression (&x).
 *
 * In TOP, & always denotes a borrow reference.
 */
class ASTBorrowExpr : public ASTExpr {
	std::shared_ptr<ASTExpr> VAR;

public:
	std::vector<std::shared_ptr<ASTNode>> getChildren() override;
	ASTBorrowExpr(std::shared_ptr<ASTExpr> VAR) : VAR(VAR) {}
	ASTExpr *getVar() const { return VAR.get(); }
	void accept(ASTVisitor *visitor) override;

protected:
	std::ostream &print(std::ostream &out) const override;
};

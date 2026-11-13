#pragma once

#include "ASTDeclNode.h"
#include "ASTExpr.h"
#include "ASTDeclStmt.h"
#include "ASTNode.h"
#include "ASTStmt.h"

/*! \brief Class for defining the signature, local declarations, and a body of a
 * function.
 */
class ASTFunction : public ASTNode {
  std::shared_ptr<ASTDeclNode> DECL;
  std::vector<std::shared_ptr<ASTDeclNode>> FORMALS;
  std::vector<std::shared_ptr<ASTDeclStmt>> DECLS;
  std::vector<std::shared_ptr<ASTStmt>> BODY;
  bool ISPOLY;
  // VERUTOP: the contract.  RESULT declares `result` for the ensures clauses.
  std::vector<std::shared_ptr<ASTExpr>> REQUIRES, ENSURES;
  std::shared_ptr<ASTDeclNode> RESULT;
  // VERUTOP: a spec function exists only for specifications; DECREASES is
  // the termination measure of a recursive function.
  bool ISSPEC = false;
  std::shared_ptr<ASTExpr> DECREASES;

public:
  std::vector<std::shared_ptr<ASTNode>> getChildren() override;
  ASTFunction(std::shared_ptr<ASTDeclNode> DECL,
              std::vector<std::shared_ptr<ASTDeclNode>> FORMALS,
              const std::vector<std::shared_ptr<ASTDeclStmt>> &DECLS,
              std::vector<std::shared_ptr<ASTStmt>> BODY, bool ISPOLY)
      : DECL(DECL), FORMALS(FORMALS), DECLS(DECLS), BODY(BODY), ISPOLY(ISPOLY) {
  }
  ~ASTFunction() = default;
  ASTDeclNode *getDecl() const { return DECL.get(); };
  const std::string &getName() const { return DECL->getName(); };
  //! Append a synthetic local declaration (used by for-loop desugaring).
  void addDecl(std::shared_ptr<ASTDeclStmt> d) { DECLS.push_back(std::move(d)); }
  bool isPoly() const { return ISPOLY; };
  void setContract(std::vector<std::shared_ptr<ASTExpr>> req,
                   std::vector<std::shared_ptr<ASTExpr>> ens,
                   std::shared_ptr<ASTDeclNode> result) {
    REQUIRES = std::move(req);
    ENSURES = std::move(ens);
    RESULT = std::move(result);
  }
  std::vector<ASTExpr *> getRequires() const {
    std::vector<ASTExpr *> v;
    for (auto &e : REQUIRES) v.push_back(e.get());
    return v;
  }
  std::vector<ASTExpr *> getEnsures() const {
    std::vector<ASTExpr *> v;
    for (auto &e : ENSURES) v.push_back(e.get());
    return v;
  }
  ASTDeclNode *getResultDecl() const { return RESULT.get(); }
  void setSpecFunction(bool isSpec) { ISSPEC = isSpec; }
  bool isSpec() const { return ISSPEC; }
  void setDecreases(std::shared_ptr<ASTExpr> d) { DECREASES = std::move(d); }
  ASTExpr *getDecreases() const { return DECREASES.get(); }
  std::vector<ASTDeclNode *> getFormals() const;
  std::vector<ASTDeclStmt *> getDeclarations() const;
  std::vector<ASTStmt *> getStmts() const;
  /*! \brief Insert \p stmt into BODY immediately before the return statement.
   *
   * Called by DestructionPass to inject ASTDestroyStmt nodes at scope exit.
   */
  void insertBeforeReturn(std::shared_ptr<ASTStmt> stmt);
  void accept(ASTVisitor *visitor) override;

  bool replaceChild(ASTNode *oldChild,
                    std::shared_ptr<ASTNode> newChild) override {
    return astReplaceVec(BODY, oldChild, newChild);
  }

protected:
  std::ostream &print(std::ostream &out) const override;
};

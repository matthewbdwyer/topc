#pragma once

#include "ASTVisitor.h"
#include <map>
#include <string>
#include <vector>

/*! \class LocalNameCollector
 *  \brief Records local names declared in each function and checks for errors.
 *
 * The local name pass visits a restricted set of nodes:
 * \sa Function to create the instance of the local map and make it current.
 * \sa DeclNode to install declared names in the current local map and
 * to check that a name is declared at most once.
 * \sa VariableExpr to ensure that the referenced name is in the map.
 * Errors are reported by throwing SemanticError exceptions.
 * \sa SemanticError
 */
class LocalNameCollector : public ASTVisitor {
  std::map<std::string, ASTDeclNode *> curMap;
  std::map<std::string, std::pair<ASTDeclNode *, bool>> fMap;
  std::string funName;
  bool first = true;
  // Scoping stacks for lexically-scoped constructs
  std::vector<std::string> forVarStack;          // for-loop iteration variable names
  std::vector<std::vector<std::string>> caseArmBindingStack; // case arm binding names

public:
  LocalNameCollector(std::map<std::string, std::pair<ASTDeclNode *, bool>> fMap)
      : fMap(fMap) {}

  // this map is public so that the static method can access it
  std::map<ASTDeclNode *, std::map<std::string, ASTDeclNode *>> lMap;

  static std::map<ASTDeclNode *, std::map<std::string, ASTDeclNode *>>
  build(ASTProgram *p,
        std::map<std::string, std::pair<ASTDeclNode *, bool>> fMap);

  virtual bool visit(ASTFunction *element) override;
  virtual void endVisit(ASTFunction *element) override;
  virtual void endVisit(ASTDeclNode *element) override;
  virtual void endVisit(ASTVariableExpr *element) override;

  // Skip sum type declarations — their DeclNode children are not local variables
  virtual bool visit(ASTSumTypeDecl *element) override { return false; }

  // For-loop variable scoping
  virtual bool visit(ASTForStmt *element) override;
  virtual void endVisit(ASTForStmt *element) override;

  // Case arm binding scoping
  virtual bool visit(ASTCaseArm *element) override;
  virtual void endVisit(ASTCaseArm *element) override;
};

#include "LocalNameCollector.h"
#include "SemanticError.h"
#include "loguru.hpp"

std::map<ASTDeclNode *, std::map<std::string, ASTDeclNode *>>
LocalNameCollector::build(
    ASTProgram *p, std::map<std::string, std::pair<ASTDeclNode *, bool>> fMap) {
  LocalNameCollector visitor(fMap);
  p->accept(&visitor);
  return visitor.lMap;
}

bool LocalNameCollector::visit(ASTFunction *element) {
  curMap.clear();
  funName = element->getName();
  first = true;
  return true;
}

void LocalNameCollector::endVisit(ASTFunction *element) {
  auto decl = element->getDecl();
  LOG_S(1) << "Adding fun [[" << decl->getName() << "@" << decl->getLine()
           << ":" << decl->getColumn() << "]] to symbol table.";
  lMap.insert(std::pair<ASTDeclNode *, std::map<std::string, ASTDeclNode *>>(
      decl, curMap));
}

void LocalNameCollector::endVisit(ASTDeclNode *element) {
  if (first) {
    // first declaration found in visiting a function is the function name which
    // is in the function map so we skip it
    first = false;
  } else {
    if (fMap.count(element->getName()) == 0) {
      if (curMap.count(element->getName()) == 0) {
        LOG_S(1) << "Adding var [[" << element->getName() << "@"
                 << element->getLine() << ":" << element->getColumn()
                 << "]] to symbol table.";
        curMap.insert(
            std::pair<std::string, ASTDeclNode *>(element->getName(), element));
      } else {
        throw SemanticError(
            "Symbol error line " + std::to_string(element->getLine()) +
            " in column " + std::to_string(element->getColumn()) + ": " +
            element->getName() + " redeclared in function " + funName + "\n");
      }
    } else {
      throw SemanticError(
          "Symbol error line " + std::to_string(element->getLine()) +
          " in column " + std::to_string(element->getColumn()) + ": " +
          element->getName() + " already declared as function\n");
    }
  }
}

void LocalNameCollector::endVisit(ASTVariableExpr *element) {
  if (fMap.count(element->getName()) == 0) {
    if (curMap.count(element->getName()) == 0) {
      throw SemanticError(
          "Symbol error line " + std::to_string(element->getLine()) +
          " in column " + std::to_string(element->getColumn()) + ": " +
          element->getName() + " undeclared in function " + funName + "\n");
    }
  }
}

// ---- For-loop variable scoping ----

bool LocalNameCollector::visit(ASTForStmt *element) {
  // The for-loop variable will be added to curMap by endVisit(ASTDeclNode*).
  // We note its name here so we can remove it when the loop ends.
  forVarStack.push_back(element->getVar()->getName());
  return true;
}

void LocalNameCollector::endVisit(ASTForStmt *element) {
  if (!forVarStack.empty()) {
    curMap.erase(forVarStack.back());
    forVarStack.pop_back();
  }
}

// ---- Case arm binding scoping ----

bool LocalNameCollector::visit(ASTCaseArm *element) {
  // Collect the names of bindings in this arm so we can remove them afterward.
  std::vector<std::string> names;
  for (auto b : element->getBindings())
    names.push_back(b->getName());
  caseArmBindingStack.push_back(std::move(names));
  return true;
}

void LocalNameCollector::endVisit(ASTCaseArm *element) {
  if (!caseArmBindingStack.empty()) {
    for (auto &n : caseArmBindingStack.back())
      curMap.erase(n);
    caseArmBindingStack.pop_back();
  }
}

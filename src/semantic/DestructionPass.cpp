#include "DestructionPass.h"

#include "ASTAssignStmt.h"
#include "ASTBlockStmt.h"
#include "ASTCaseStmt.h"
#include "ASTDestroyStmt.h"
#include "ASTFunction.h"
#include "ASTIfStmt.h"
#include "ASTProgram.h"
#include "ASTReturnStmt.h"
#include "ASTVariableExpr.h"
#include "ASTWhileStmt.h"

#include <algorithm>
#include <vector>

// ---------------------------------------------------------------------------
// Constructor / static entry point
// ---------------------------------------------------------------------------

DestructionPass::DestructionPass(SymbolTable *sym, OwnershipClassifier *oc)
    : sym(sym), classifier(oc) {}

void DestructionPass::run(ASTProgram *ast, SymbolTable *sym,
                          OwnershipClassifier *oc) {
  DestructionPass pass(sym, oc);
  for (auto *f : ast->getFunctions()) {
    pass.processFunction(f);
  }
}

// ---------------------------------------------------------------------------
// Per-function processing
// ---------------------------------------------------------------------------

void DestructionPass::processFunction(ASTFunction *f) {
  currentFuncDecl = f->getDecl();

  // Initial state: Own parameters begin as Owned; Own locals have no entry
  // (they are uninitialized until first assigned, which sets them to Owned).
  StateMap state;
  for (auto *param : f->getFormals()) {
    if (classifier->classify(param) == OwnershipClass::Own) {
      state[param] = OwnershipState::Owned;
    }
  }

  // Forward-simulate ownership through all statements (including return).
  for (auto *stmt : f->getStmts()) {
    state = analyzeStmt(stmt, std::move(state));
  }

  // Collect every Own variable that is still Owned at function exit.
  std::vector<ASTDeclNode *> toDestroy;
  for (auto &[decl, s] : state) {
    if (s == OwnershipState::Owned) {
      toDestroy.push_back(decl);
    }
  }

  // Sort by name for deterministic insertion order.
  std::sort(toDestroy.begin(), toDestroy.end(),
            [](ASTDeclNode *a, ASTDeclNode *b) {
              return a->getName() < b->getName();
            });

  for (auto *decl : toDestroy) {
    f->insertBeforeReturn(std::make_shared<ASTDestroyStmt>(decl));
  }
}

// ---------------------------------------------------------------------------
// Statement transfer function (no error checking — program already validated)
// ---------------------------------------------------------------------------

DestructionPass::StateMap DestructionPass::analyzeStmt(ASTStmt *stmt,
                                                        StateMap state) {
  // Assignment
  if (auto *assign = dynamic_cast<ASTAssignStmt *>(stmt)) {
    return analyzeAssign(assign, std::move(state));
  }

  // Block: sequential composition
  if (auto *block = dynamic_cast<ASTBlockStmt *>(stmt)) {
    for (auto *s : block->getStmts()) {
      state = analyzeStmt(s, std::move(state));
    }
    return state;
  }

  // If statement — MoveAnalysis guarantees both branches agree at the join.
  if (auto *ifStmt = dynamic_cast<ASTIfStmt *>(stmt)) {
    auto thenState = analyzeStmt(ifStmt->getThen(), state);
    // Either branch is equivalent at the join; return thenState.
    return thenState;
  }

  // While — MoveAnalysis guarantees the body does not change Own state.
  if (dynamic_cast<ASTWhileStmt *>(stmt)) {
    return state;
  }

  // Return: if the returned value is a direct Own variable, mark it Moved.
  if (auto *retStmt = dynamic_cast<ASTReturnStmt *>(stmt)) {
    auto *retVar = dynamic_cast<ASTVariableExpr *>(retStmt->getArg());
    if (retVar) {
      ASTDeclNode *decl = resolveVar(retVar->getName());
      if (decl && classifier->classify(decl) == OwnershipClass::Own) {
        state[decl] = OwnershipState::Moved;
      }
    }
    return state;
  }

  // Case statement: all arms must agree (guaranteed by MoveAnalysis).
  if (auto *caseStmt = dynamic_cast<ASTCaseStmt *>(stmt)) {
    auto arms = caseStmt->getArms();
    if (!arms.empty()) {
      return analyzeStmt(arms[0]->getBody(), state);
    }
    return state;
  }

  // Output, Error, ASTDestroyStmt, DeclStmt, etc.: no ownership state change.
  return state;
}

// ---------------------------------------------------------------------------
// Assignment transfer function
// ---------------------------------------------------------------------------

DestructionPass::StateMap DestructionPass::analyzeAssign(ASTAssignStmt *stmt,
                                                          StateMap state) {
  auto *rhsVar = dynamic_cast<ASTVariableExpr *>(stmt->getRHS());
  ASTDeclNode *rhsDecl = rhsVar ? resolveVar(rhsVar->getName()) : nullptr;
  bool rhsIsOwn =
      rhsDecl && classifier->classify(rhsDecl) == OwnershipClass::Own;

  if (rhsIsOwn) {
    // Transfer: source becomes Moved.
    state[rhsDecl] = OwnershipState::Moved;
  }

  auto *lhsVar = dynamic_cast<ASTVariableExpr *>(stmt->getLHS());
  ASTDeclNode *lhsDecl = lhsVar ? resolveVar(lhsVar->getName()) : nullptr;
  bool lhsIsOwn =
      lhsDecl && classifier->classify(lhsDecl) == OwnershipClass::Own;

  if (lhsIsOwn) {
    // Destination becomes Owned.
    state[lhsDecl] = OwnershipState::Owned;
  }

  return state;
}

// ---------------------------------------------------------------------------
// Variable name resolver
// ---------------------------------------------------------------------------

ASTDeclNode *DestructionPass::resolveVar(const std::string &name) const {
  if (currentFuncDecl) {
    auto *local = sym->getLocal(name, currentFuncDecl);
    if (local)
      return local;
  }
  return sym->getFunction(name);
}

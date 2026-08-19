#include "DestructionPass.h"
#include "../SemanticLogging.h"

#include "ASTAssignStmt.h"
#include "ASTAllocExpr.h"
#include "ASTBorrowExpr.h"
#include "ASTBlockStmt.h"
#include "ASTCaseStmt.h"
#include "ASTDestroyStmt.h"
#include "ASTErrorStmt.h"
#include "ASTFunction.h"
#include "ASTFunAppExpr.h"
#include "ASTIfStmt.h"
#include "ASTOutputStmt.h"
#include "ASTProgram.h"
#include "ASTReturnStmt.h"
#include "ASTVariableExpr.h"
#include "ASTWhileStmt.h"

#include <algorithm>
#include <vector>

// ---------------------------------------------------------------------------
// Constructor / static entry point
// ---------------------------------------------------------------------------

DestructionPass::DestructionPass(SymbolTable *sym, OwnershipClassifier *oc,
                                 FunctionEffectSummaries *effects)
    : sym(sym), classifier(oc), functionEffects(effects) {}

void DestructionPass::run(ASTProgram *ast, SymbolTable *sym,
                          OwnershipClassifier *oc,
                          FunctionEffectSummaries *effects) {
  SEMANTIC_LOG(1, "destruction") << "start";
  DestructionPass pass(sym, oc, effects);
  for (auto *f : ast->getFunctions()) {
    pass.processFunction(f);
  }
  SEMANTIC_LOG(1, "destruction") << "complete";
}

// ---------------------------------------------------------------------------
// Per-function processing
// ---------------------------------------------------------------------------

void DestructionPass::processFunction(ASTFunction *f) {
  currentFuncDecl = f->getDecl();
  currentFormals.clear();
  for (auto *param : f->getFormals()) {
    currentFormals.insert(param);
  }

  // Initial state: Own locals have no entry (uninitialized until first
  // assigned). An owned formal is owned by the callee (the caller moved it in
  // by value); destroy it at scope exit unless it is moved out.
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

  // Collect every Own variable (local or consumed formal) still Owned at exit.
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
    SEMANTIC_LOG(2, "destruction")
        << "function=" << f->getName() << " insert variable="
        << decl->getName();
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
    consumeCallArgMoves(ifStmt->getCondition(), state);
    auto thenState = analyzeStmt(ifStmt->getThen(), state);
    // Either branch is equivalent at the join; return thenState.
    return thenState;
  }

  // While — MoveAnalysis guarantees the body does not change Own state.
  if (auto *whileStmt = dynamic_cast<ASTWhileStmt *>(stmt)) {
    consumeCallArgMoves(whileStmt->getCondition(), state);
    return state;
  }

  // Return: if the returned value is a direct Own variable, mark it Moved.
  if (auto *retStmt = dynamic_cast<ASTReturnStmt *>(stmt)) {
    consumeCallArgMoves(retStmt->getArg(), state);
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
    consumeCallArgMoves(caseStmt->getCaseExpr(), state);
    auto arms = caseStmt->getArms();
    if (!arms.empty()) {
      return analyzeStmt(arms[0]->getBody(), state);
    }
    return state;
  }

  // Output / Error: consume owned call-argument moves in the expression.
  if (auto *outputStmt = dynamic_cast<ASTOutputStmt *>(stmt)) {
    consumeCallArgMoves(outputStmt->getArg(), state);
    return state;
  }
  if (auto *errorStmt = dynamic_cast<ASTErrorStmt *>(stmt)) {
    consumeCallArgMoves(errorStmt->getArg(), state);
    return state;
  }

  // ASTDestroyStmt, DeclStmt, etc.: no ownership state change.
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
  } else {
    consumeCallArgMoves(stmt->getRHS(), state);
  }

  auto *lhsVar = dynamic_cast<ASTVariableExpr *>(stmt->getLHS());
  ASTDeclNode *lhsDecl = lhsVar ? resolveVar(lhsVar->getName()) : nullptr;
  bool lhsIsOwn =
      lhsDecl && classifier->classify(lhsDecl) == OwnershipClass::Own;

  bool rhsIsBorrowExpr =
      dynamic_cast<ASTBorrowExpr *>(stmt->getRHS()) != nullptr;

  if (lhsIsOwn) {
    if (rhsIsBorrowExpr) {
      // Borrow-derived values are aliases and must not be auto-destroyed.
      state.erase(lhsDecl);
      return state;
    }

    // Trust the solved type: an owning-typed binding is Owned. Linear ownership
    // guarantees the call result is uniquely owned; MoveAnalysis invalidates the
    // source, so no summary re-derivation is needed.
    state[lhsDecl] = OwnershipState::Owned;
  }

  return state;
}

bool DestructionPass::actualInstantiatesOwn(
    const ASTExpr *actual, FunctionEffectSummaries::FormalMode mode,
    bool lhsIsOwnFallback) const {
  switch (mode) {
  case FunctionEffectSummaries::FormalMode::Own:
    return true;
  case FunctionEffectSummaries::FormalMode::Copy:
    return false;
  case FunctionEffectSummaries::FormalMode::DependsOnInstantiation: {
    auto *argVar = dynamic_cast<const ASTVariableExpr *>(actual);
    ASTDeclNode *argDecl =
        (argVar != nullptr) ? resolveVar(argVar->getName()) : nullptr;
    if (argDecl != nullptr) {
      return classifier->classify(argDecl) == OwnershipClass::Own;
    }

    if (dynamic_cast<const ASTAllocExpr *>(actual) != nullptr) {
      return true;
    }

    return lhsIsOwnFallback;
  }
  }

  return false;
}

void DestructionPass::consumeCallArgMoves(ASTNode *node, StateMap &state) {
  if (node == nullptr) {
    return;
  }

  if (auto *call = dynamic_cast<ASTFunAppExpr *>(node)) {
    auto *calleeVar = dynamic_cast<ASTVariableExpr *>(call->getFunction());
    ASTDeclNode *calleeDecl =
        calleeVar != nullptr ? sym->getFunction(calleeVar->getName()) : nullptr;
    const FunctionEffectSummaries::Summary *summary =
        calleeDecl != nullptr && functionEffects != nullptr
            ? functionEffects->get(calleeDecl)
            : nullptr;

    auto actuals = call->getActuals();
    const std::size_t count =
        summary != nullptr
            ? std::min(actuals.size(), summary->formalModes.size())
            : 0;
    for (std::size_t index = 0; index < count; ++index) {
      if (!actualInstantiatesOwn(actuals[index], summary->formalModes[index],
                                 false)) {
        continue;
      }

      auto *actualVar = dynamic_cast<ASTVariableExpr *>(actuals[index]);
      ASTDeclNode *actualDecl = actualVar != nullptr
                                    ? resolveVar(actualVar->getName())
                                    : nullptr;
      if (actualDecl != nullptr &&
          classifier->classify(actualDecl) == OwnershipClass::Own) {
        state[actualDecl] = OwnershipState::Moved;
      }
    }
  }

  for (auto &child : node->getChildren()) {
    consumeCallArgMoves(child.get(), state);
  }
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

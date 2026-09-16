#include "ASTSumCtorExpr.h"
#include "DestructionPass.h"
#include "RuleToggles.h"
#include "../SemanticLogging.h"

#include "ASTAssignStmt.h"
#include "ASTAllocExpr.h"
#include "ASTBorrowExpr.h"
#include "ASTBlockStmt.h"
#include "ASTCaseArm.h"
#include "ASTCaseStmt.h"
#include "ASTDeRefExpr.h"
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
#include <optional>
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

  if (!RuleToggles::enabled("destroy-at-exit")) {
    toDestroy.clear();
  }
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

  // If statement — MoveAnalysis guarantees the branches agree on which Own
  // variables are Owned; join them the same way it does.
  if (auto *ifStmt = dynamic_cast<ASTIfStmt *>(stmt)) {
    consumeCallArgMoves(ifStmt->getCondition(), state);
    auto thenState = analyzeStmt(ifStmt->getThen(), state);
    StateMap elseState = ifStmt->getElse() != nullptr
                             ? analyzeStmt(ifStmt->getElse(), state)
                             : state;
    return MoveAnalysis::joinStates({thenState, elseState}, false);
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
    // An owned by-value scrutinee is consumed by the match (its box is freed by
    // code generation), so it must not also be destroyed at scope exit.
    auto *scrutVar =
        dynamic_cast<ASTVariableExpr *>(caseStmt->getCaseExpr());
    ASTDeclNode *scrutDecl =
        scrutVar ? resolveVar(scrutVar->getName()) : nullptr;
    if (scrutDecl && classifier->classify(scrutDecl) == OwnershipClass::Own) {
      state[scrutDecl] = OwnershipState::Moved;
    }
    bool byValue =
        dynamic_cast<ASTDeRefExpr *>(caseStmt->getCaseExpr()) == nullptr;
    std::vector<StateMap> armStates;
    for (auto *arm : caseStmt->getArms()) {
      armStates.push_back(processArm(arm, byValue, state));
    }
    if (armStates.empty()) {
      return state; // LCOV_EXCL_LINE -- the parser requires at least one arm
    }
    return MoveAnalysis::joinStates(armStates, false);
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
// Case arms: free owned binders that are still Owned at the end of the arm
// ---------------------------------------------------------------------------

DestructionPass::StateMap DestructionPass::processArm(ASTCaseArm *arm,
                                                      bool byValue,
                                                      StateMap state) {
  // Same keying as MoveAnalysis::analyzeArm: state under the declaration that
  // uses resolve to; the destroy names the arm's own binding, whose alloca and
  // type are the ones code generation has in scope for the arm.
  struct Binder {
    ASTDeclNode *binding;
    ASTDeclNode *key;
    std::optional<OwnershipState> previous;
  };
  std::vector<Binder> binders;
  for (auto *binding : MoveAnalysis::ownedBinders(arm, byValue, classifier)) {
    ASTDeclNode *key = resolveVar(binding->getName());
    if (key == nullptr) {
      key = binding; // LCOV_EXCL_LINE -- every binding is registered by name
    }
    auto it = state.find(key);
    binders.push_back({binding, key,
                       it != state.end()
                           ? std::optional<OwnershipState>(it->second)
                           : std::nullopt});
    state[key] = OwnershipState::Owned;
  }
  state = analyzeStmt(arm->getBody(), std::move(state));

  std::vector<ASTDeclNode *> toDestroy;
  for (auto &binder : binders) {
    auto it = state.find(binder.key);
    if (it != state.end() && it->second == OwnershipState::Owned) {
      toDestroy.push_back(binder.binding);
    }
    if (binder.previous.has_value()) {
      state[binder.key] = *binder.previous;
    } else {
      state.erase(binder.key);
    }
  }
  if (toDestroy.empty() || !RuleToggles::enabled("destroy-arm-binders")) {
    return state;
  }

  // The binders are in scope only inside the arm, so their destroys go at the
  // end of the arm's body: wrap the body in a block that ends with them.
  std::shared_ptr<ASTStmt> body;
  for (auto &child : arm->getChildren()) {
    if (child.get() == arm->getBody()) {
      body = std::dynamic_pointer_cast<ASTStmt>(child);
    }
  }
  std::vector<std::shared_ptr<ASTStmt>> stmts{body};
  for (auto *binder : toDestroy) {
    SEMANTIC_LOG(2, "destruction")
        << "function=" << currentFuncDecl->getName()
        << " insert arm-binder=" << binder->getName();
    stmts.push_back(std::make_shared<ASTDestroyStmt>(binder));
  }
  auto block = std::make_shared<ASTBlockStmt>(std::move(stmts));
  arm->replaceChild(arm->getBody(), block);
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

void DestructionPass::consumeCallArgMoves(ASTNode *node, StateMap &state) {
  if (node == nullptr) {
    return;
  }

  if (auto *call = dynamic_cast<ASTFunAppExpr *>(node)) {
    // Same decision MoveAnalysis used: FunctionEffectSummaries::callEffect.
    const FunctionEffectSummaries::CallEffect *effect =
        functionEffects != nullptr ? functionEffects->callEffect(call) : nullptr;
    auto actuals = call->getActuals();
    std::size_t n =
        effect != nullptr ? std::min(actuals.size(), effect->consumes.size()) : 0;

    for (std::size_t i = 0; i < n; ++i) {
      if (!effect->consumes[i]) {
        continue;
      }
      auto *actualVar = dynamic_cast<ASTVariableExpr *>(actuals[i]);
      ASTDeclNode *actualDecl =
          actualVar != nullptr ? resolveVar(actualVar->getName()) : nullptr;
      if (actualDecl != nullptr &&
          classifier->classify(actualDecl) == OwnershipClass::Own) {
        state[actualDecl] = OwnershipState::Moved;
      }
    }
  }

  // A constructor payload takes ownership of an Own variable (see
  // MoveAnalysis::consumeCallArgMoves): the variable is Moved and must not be
  // destroyed at scope exit.
  if (auto *ctor = dynamic_cast<ASTSumCtorExpr *>(node)) {
    for (auto *payload : ctor->getArgs()) {
      auto *payloadVar = dynamic_cast<ASTVariableExpr *>(payload);
      ASTDeclNode *decl =
          payloadVar != nullptr ? resolveVar(payloadVar->getName()) : nullptr;
      if (decl != nullptr &&
          classifier->classify(decl) == OwnershipClass::Own) {
        state[decl] = OwnershipState::Moved;
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

#include "ASTSumCtorExpr.h"
#include "MoveAnalysis.h"
#include "../SemanticLogging.h"

#include "ASTAssignStmt.h"
#include "ASTAllocExpr.h"
#include "ASTBorrowExpr.h"
#include "ASTBlockStmt.h"
#include "ASTCaseStmt.h"
#include "ASTErrorStmt.h"
#include "ASTFunction.h"
#include "ASTFunAppExpr.h"
#include "ASTIfStmt.h"
#include "ASTOutputStmt.h"
#include "ASTProgram.h"
#include "ASTReturnStmt.h"
#include "ASTVariableExpr.h"
#include "ASTWhileStmt.h"
#include "SemanticError.h"

#include <set>
#include <sstream>

std::vector<MoveAnalysis::MoveTraceEvent> MoveAnalysis::lastTrace;

// ---------------------------------------------------------------------------
// Constructor: run analysis over every function.
// ---------------------------------------------------------------------------

MoveAnalysis::MoveAnalysis(ASTProgram *ast, SymbolTable *sym,
                           OwnershipClassifier *oc,
                           FunctionEffectSummaries *effects)
  : program(ast), sym(sym), classifier(oc), functionEffects(effects),
    currentFuncDecl(nullptr) {
  SEMANTIC_LOG(1, "move-analysis") << "start";
  for (auto *f : ast->getFunctions()) {
    analyzeFunction(f);
  }
  lastTrace = trace;
  SEMANTIC_LOG(1, "move-analysis")
      << "complete events=" << trace.size();
}

const std::vector<MoveAnalysis::MoveTraceEvent> &MoveAnalysis::getLastTrace() {
  return lastTrace;
}

// ---------------------------------------------------------------------------
// Per-function analysis
// ---------------------------------------------------------------------------

void MoveAnalysis::analyzeFunction(ASTFunction *f) {
  currentFuncDecl = f->getDecl();
  currentFormals.clear();
  for (auto *param : f->getFormals()) {
    currentFormals.insert(param);
  }

  // Initial state: Own locals start uninitialized (no entry in the map). An
  // owned formal is owned by the callee (the caller moved it in by value), so
  // it starts Owned -- exactly as the destruction pass models it -- and is
  // therefore subject to the same double-move and join-agreement checks.
  StateMap state;
  for (auto *param : f->getFormals()) {
    if (classifier->classify(param) == OwnershipClass::Own) {
      state[param] = OwnershipState::Owned;
    }
  }

  // Analyse each statement in the function body.
  for (auto *stmt : f->getStmts()) {
    state = analyzeStmt(stmt, std::move(state));
  }
}

// ---------------------------------------------------------------------------
// Statement transfer function
// ---------------------------------------------------------------------------

MoveAnalysis::StateMap MoveAnalysis::analyzeStmt(ASTStmt *stmt, StateMap state) {
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

  // If statement
  if (auto *ifStmt = dynamic_cast<ASTIfStmt *>(stmt)) {
    checkExprForMoved(ifStmt->getCondition(), state);
    consumeCallArgMoves(ifStmt->getCondition(), state);
    auto thenState = analyzeStmt(ifStmt->getThen(), state);
    StateMap elseState =
        (ifStmt->getElse() != nullptr)
            ? analyzeStmt(ifStmt->getElse(), state)
            : state; // implicit else: state unchanged
    assertJoinAgrees(state, thenState, elseState);
    return thenState; // both agree, so either is fine
  }

  // While loop: body may not change any Own variable's state
  if (auto *whileStmt = dynamic_cast<ASTWhileStmt *>(stmt)) {
    checkExprForMoved(whileStmt->getCondition(), state);
    consumeCallArgMoves(whileStmt->getCondition(), state);
    auto bodyState = analyzeStmt(whileStmt->getBody(), state);
    // Collect all keys in bodyState that differ from preState
    for (auto &[decl, s] : bodyState) {
      auto it = state.find(decl);
      OwnershipState pre =
          (it != state.end()) ? it->second : OwnershipState::Moved;
      if (s != pre) {
        std::ostringstream oss;
        oss << "move inside while-loop body on line "
            << whileStmt->getLine();
        throw SemanticError(oss.str());
      }
    }
    return state;
  }

  // Return: check expression, mark directly-returned Own variables as Moved
  if (auto *retStmt = dynamic_cast<ASTReturnStmt *>(stmt)) {
    checkExprForMoved(retStmt->getArg(), state);
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

  // Output / Error: check expression, no ownership state change
  if (auto *outputStmt = dynamic_cast<ASTOutputStmt *>(stmt)) {
    checkExprForMoved(outputStmt->getArg(), state);
    consumeCallArgMoves(outputStmt->getArg(), state);
    return state;
  }
  if (auto *errorStmt = dynamic_cast<ASTErrorStmt *>(stmt)) {
    checkExprForMoved(errorStmt->getArg(), state);
    consumeCallArgMoves(errorStmt->getArg(), state);
    return state;
  }

  // Case statement: pattern-match on sum type
  if (auto *caseStmt = dynamic_cast<ASTCaseStmt *>(stmt)) {
    checkExprForMoved(caseStmt->getCaseExpr(), state);
    consumeCallArgMoves(caseStmt->getCaseExpr(), state);
    // Matching an owned by-value scrutinee consumes it: its payloads are moved
    // out into the arm bindings and its box is freed by the match. A borrowed
    // scrutinee (`case *p`) is not consumed.
    auto *scrutVar =
        dynamic_cast<ASTVariableExpr *>(caseStmt->getCaseExpr());
    ASTDeclNode *scrutDecl =
        scrutVar ? resolveVar(scrutVar->getName()) : nullptr;
    if (scrutDecl && classifier->classify(scrutDecl) == OwnershipClass::Own) {
      state[scrutDecl] = OwnershipState::Moved;
    }
    std::vector<StateMap> armStates;
    for (auto *arm : caseStmt->getArms()) {
      armStates.push_back(analyzeStmt(arm->getBody(), state));
    }
    // All arms must agree on Own variable states
    if (!armStates.empty()) {
      for (std::size_t i = 1; i < armStates.size(); ++i) {
        assertJoinAgrees(state, armStates[0], armStates[i]);
      }
      return armStates[0];
    }
    return state;
  }

  // Unknown statement type: no ownership state change.
  return state;
}

// ---------------------------------------------------------------------------
// Assignment transfer function
// ---------------------------------------------------------------------------

MoveAnalysis::StateMap MoveAnalysis::analyzeAssign(ASTAssignStmt *stmt,
                                                    StateMap state) {
  ASTExpr *lhs = stmt->getLHS();
  ASTExpr *rhs = stmt->getRHS();

  // Determine if the RHS is a direct Own variable reference (a move).
  auto *rhsVar = dynamic_cast<ASTVariableExpr *>(rhs);
  ASTDeclNode *rhsDecl = rhsVar ? resolveVar(rhsVar->getName()) : nullptr;
  bool rhsIsOwn =
      rhsDecl && classifier->classify(rhsDecl) == OwnershipClass::Own;

  if (rhsIsOwn) {
    // Check for double-move before checkExprForMoved to emit the right message.
    auto rhsIt = state.find(rhsDecl);
    if (rhsIt != state.end() && rhsIt->second == OwnershipState::Moved) {
      std::ostringstream oss;
      oss << "variable '" << rhsVar->getName()
          << "' moved more than once on line " << stmt->getLine();
      throw SemanticError(oss.str());
    }
    // Transfer ownership: mark rhs as Moved.
    state[rhsDecl] = OwnershipState::Moved;
    trace.push_back({"move", rhsVar->getName(), stmt->getLine(),
                     "ownership moved from RHS variable"});
    SEMANTIC_LOG(2, "move-analysis")
      << "line=" << stmt->getLine() << " event=move variable="
      << rhsVar->getName() << " reason=rhs-variable";
  } else {
    // Non-move assignment: check full RHS expression for any use-after-move.
    checkExprForMoved(rhs, state);
    consumeCallArgMoves(rhs, state);
  }

  // Determine if the LHS is a direct Own variable reference.
  auto *lhsVar = dynamic_cast<ASTVariableExpr *>(lhs);
  ASTDeclNode *lhsDecl = lhsVar ? resolveVar(lhsVar->getName()) : nullptr;
  bool lhsIsOwn =
      lhsDecl && classifier->classify(lhsDecl) == OwnershipClass::Own;

  bool rhsIsBorrowExpr = dynamic_cast<ASTBorrowExpr *>(rhs) != nullptr;

  if (lhsIsOwn) {
    if (rhsIsBorrowExpr) {
      // Borrow expressions do not transfer heap ownership to the destination.
      state.erase(lhsDecl);
      return state;
    }

    // Trust the solved type: an owning-typed binding receives ownership here.
    auto lhsIt = state.find(lhsDecl);
    if (lhsIt != state.end() && lhsIt->second == OwnershipState::Owned) {
      // Assign-over-live-own: hard error.
      std::ostringstream oss;
      oss << "variable '" << lhsVar->getName()
          << "' assigned while still owned on line " << stmt->getLine()
          << " — free or move first";
      throw SemanticError(oss.str());
    }
    // LHS becomes Owned.
    state[lhsDecl] = OwnershipState::Owned;
    trace.push_back({"own", lhsVar->getName(), stmt->getLine(),
                     rhsIsOwn ? "ownership received via move"
                              : "ownership established by assignment"});
    SEMANTIC_LOG(2, "move-analysis")
      << "line=" << stmt->getLine() << " event=own variable="
      << lhsVar->getName();
  }

  return state;
}

bool MoveAnalysis::actualInstantiatesOwn(
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

    // Allocation expressions are always owning in TOP.
    if (dynamic_cast<const ASTAllocExpr *>(actual) != nullptr) {
      return true;
    }

    // For non-variable/non-alloc expressions, fall back to the LHS ownership
    // expectation to avoid dropping ownership on polymorphic instantiations.
    return lhsIsOwnFallback;
  }
  }

  return false;
}

void MoveAnalysis::consumeCallArgMoves(ASTNode *node, StateMap &state) {
  if (node == nullptr) {
    return;
  }

  if (auto *call = dynamic_cast<ASTFunAppExpr *>(node)) {
    auto *calleeVar = dynamic_cast<ASTVariableExpr *>(call->getFunction());
    ASTDeclNode *calleeDecl =
        (calleeVar != nullptr) ? sym->getFunction(calleeVar->getName()) : nullptr;
    const FunctionEffectSummaries::Summary *summary =
        (calleeDecl != nullptr && functionEffects != nullptr)
            ? functionEffects->get(calleeDecl)
            : nullptr;

    auto actuals = call->getActuals();
    std::size_t n =
        (summary != nullptr) ? std::min(actuals.size(), summary->formalModes.size()) : 0;

    for (std::size_t i = 0; i < n; ++i) {
      bool shouldConsume = false;
      switch (summary->formalModes[i]) {
      case FunctionEffectSummaries::FormalMode::Own:
        shouldConsume = true;
        break;
      case FunctionEffectSummaries::FormalMode::Copy:
        shouldConsume = false;
        break;
      case FunctionEffectSummaries::FormalMode::DependsOnInstantiation: {
        auto *depVar = dynamic_cast<ASTVariableExpr *>(actuals[i]);
        ASTDeclNode *depDecl =
            (depVar != nullptr) ? resolveVar(depVar->getName()) : nullptr;
        shouldConsume = depDecl != nullptr &&
                        classifier->classify(depDecl) == OwnershipClass::Own;
        break;
      }
      }

      if (!shouldConsume) {
        continue;
      }

      auto *actual = actuals[i];
      auto *argVar = dynamic_cast<ASTVariableExpr *>(actual);
      if (argVar == nullptr) {
        continue;
      }

      ASTDeclNode *decl = resolveVar(argVar->getName());
      if (decl == nullptr || classifier->classify(decl) != OwnershipClass::Own) {
        continue;
      }

      auto it = state.find(decl);
      if (it != state.end() && it->second == OwnershipState::Moved) {
        std::ostringstream oss;
        oss << "variable '" << argVar->getName()
            << "' moved more than once on line " << call->getLine();
        throw SemanticError(oss.str());
      }

      state[decl] = OwnershipState::Moved;
      trace.push_back({"move", argVar->getName(), call->getLine(),
                       "ownership moved via function argument"});
        SEMANTIC_LOG(2, "move-analysis")
          << "line=" << call->getLine() << " event=move variable="
          << argVar->getName() << " reason=function-argument";
    }
  }


  // A constructor payload takes ownership of an Own variable: the box owns it
  // from here on, so the variable is moved exactly as if it were passed to a
  // consuming call.
  if (auto *ctor = dynamic_cast<ASTSumCtorExpr *>(node)) {
    for (auto *payload : ctor->getArgs()) {
      auto *payloadVar = dynamic_cast<ASTVariableExpr *>(payload);
      if (payloadVar == nullptr) {
        continue;
      }
      ASTDeclNode *decl = resolveVar(payloadVar->getName());
      if (decl == nullptr ||
          classifier->classify(decl) != OwnershipClass::Own) {
        continue;
      }
      auto it = state.find(decl);
      if (it != state.end() && it->second == OwnershipState::Moved) {
        std::ostringstream oss;
        oss << "variable '" << payloadVar->getName()
            << "' moved more than once on line " << payloadVar->getLine();
        throw SemanticError(oss.str());
      }
      state[decl] = OwnershipState::Moved;
      trace.push_back({"move", payloadVar->getName(), payloadVar->getLine(),
                       "ownership moved into constructor payload"});
      SEMANTIC_LOG(2, "move-analysis")
          << "line=" << payloadVar->getLine() << " event=move variable="
          << payloadVar->getName() << " reason=constructor-payload";
    }
  }

  for (auto &child : node->getChildren()) {
    consumeCallArgMoves(child.get(), state);
  }
}

// ---------------------------------------------------------------------------
// Expression use-after-move checker
// ---------------------------------------------------------------------------

void MoveAnalysis::checkExprForMoved(ASTNode *node,
                                      const StateMap &state) const {
  if (node == nullptr)
    return;

  auto *varExpr = dynamic_cast<ASTVariableExpr *>(node);
  if (varExpr) {
    ASTDeclNode *decl = resolveVar(varExpr->getName());
    if (decl && classifier->classify(decl) == OwnershipClass::Own) {
      auto it = state.find(decl);
      if (it != state.end() && it->second == OwnershipState::Moved) {
        std::ostringstream oss;
        oss << "variable '" << varExpr->getName()
            << "' used after move on line " << varExpr->getLine();
        throw SemanticError(oss.str());
      }
    }
  }

  for (auto &child : node->getChildren()) {
    checkExprForMoved(child.get(), state);
  }
}

// ---------------------------------------------------------------------------
// Variable name resolver
// ---------------------------------------------------------------------------

ASTDeclNode *MoveAnalysis::resolveVar(const std::string &name) const {
  // Try local (parameter or var-declared) first.
  if (currentFuncDecl) {
    auto *local = sym->getLocal(name, currentFuncDecl);
    if (local)
      return local;
  }
  // Fall back to global function name.
  return sym->getFunction(name);
}

// ---------------------------------------------------------------------------
// Join agreement check
// ---------------------------------------------------------------------------

void MoveAnalysis::assertJoinAgrees(const StateMap &preState,
                                     const StateMap &thenState,
                                     const StateMap &elseState) {
  // Only check variables that were already tracked before the branch.
  // Variables first assigned inside a branch (not in preState) need not agree —
  // they are simply uninitialized on the path that didn't assign them.
  for (auto &[decl, preOwn] : preState) {
    auto lookup = [](const StateMap &m, ASTDeclNode *d,
                     OwnershipState def) -> OwnershipState {
      auto it = m.find(d);
      return (it != m.end()) ? it->second : def;
    };

    OwnershipState thenS = lookup(thenState, decl, preOwn);
    OwnershipState elseS = lookup(elseState, decl, preOwn);

    if (thenS != elseS) {
      std::ostringstream oss;
      oss << "ownership state disagreement at control-flow join for "
             "an Own variable: one path leaves it "
          << (thenS == OwnershipState::Owned ? "Owned" : "Moved")
          << " and the other leaves it "
          << (elseS == OwnershipState::Owned ? "Owned" : "Moved");
      throw SemanticError(oss.str());
    }
  }
}

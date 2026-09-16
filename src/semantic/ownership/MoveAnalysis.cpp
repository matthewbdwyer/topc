#include "ASTSumCtorExpr.h"
#include "MoveAnalysis.h"
#include "RuleToggles.h"
#include "../SemanticLogging.h"

#include "ASTAssignStmt.h"
#include "ASTAllocExpr.h"
#include "ASTBorrowExpr.h"
#include "ASTBlockStmt.h"
#include "ASTCaseArm.h"
#include "ASTCaseStmt.h"
#include "ASTDeRefExpr.h"
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

#include <optional>
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
  movedInStmt.clear();

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

  // If statement: both branches start from the state after the condition and
  // must agree, at the join, on which Own variables are Owned.
  if (auto *ifStmt = dynamic_cast<ASTIfStmt *>(stmt)) {
    evalExpr(ifStmt->getCondition(), state);
    auto thenState = analyzeStmt(ifStmt->getThen(), state);
    StateMap elseState =
        (ifStmt->getElse() != nullptr)
            ? analyzeStmt(ifStmt->getElse(), state)
            : state; // implicit else: state unchanged
    return joinStates({thenState, elseState}, true);
  }

  // While loop: the condition runs once more than the body, so neither may
  // change any Own variable's state.
  if (auto *whileStmt = dynamic_cast<ASTWhileStmt *>(stmt)) {
    StateMap condState = state;
    evalExpr(whileStmt->getCondition(), condState);
    if (condState != state) {
      std::ostringstream oss;
      oss << "move in while-loop condition on line " << whileStmt->getLine();
      if (RuleToggles::enabled("loop-condition")) throw SemanticError(oss.str());
    }
    auto bodyState = analyzeStmt(whileStmt->getBody(), state);
    assertLoopInvariant(state, bodyState, whileStmt->getLine());
    return state;
  }

  // Return: evaluate the expression; a directly returned Own variable moves.
  if (auto *retStmt = dynamic_cast<ASTReturnStmt *>(stmt)) {
    auto *retVar = dynamic_cast<ASTVariableExpr *>(retStmt->getArg());
    ASTDeclNode *decl = retVar ? resolveVar(retVar->getName()) : nullptr;
    if (decl && classifier->classify(decl) == OwnershipClass::Own) {
      consumeVar(retVar, decl, state, "ownership moved by return");
    } else {
      evalExpr(retStmt->getArg(), state);
    }
    return state;
  }

  // Output / Error: evaluate, no ownership state change of their own
  if (auto *outputStmt = dynamic_cast<ASTOutputStmt *>(stmt)) {
    evalExpr(outputStmt->getArg(), state);
    return state;
  }
  if (auto *errorStmt = dynamic_cast<ASTErrorStmt *>(stmt)) {
    evalExpr(errorStmt->getArg(), state);
    return state;
  }

  // Case statement: pattern-match on sum type
  if (auto *caseStmt = dynamic_cast<ASTCaseStmt *>(stmt)) {
    // Matching an owned by-value scrutinee consumes it: its payloads are moved
    // out into the arm bindings and its box is freed by the match. A borrowed
    // scrutinee (`case *p`) is not consumed.
    auto *scrutVar =
        dynamic_cast<ASTVariableExpr *>(caseStmt->getCaseExpr());
    ASTDeclNode *scrutDecl =
        scrutVar ? resolveVar(scrutVar->getName()) : nullptr;
    if (scrutDecl && classifier->classify(scrutDecl) == OwnershipClass::Own) {
      consumeVar(scrutVar, scrutDecl, state, "ownership consumed by case");
    } else {
      evalExpr(caseStmt->getCaseExpr(), state);
    }
    bool byValue =
        dynamic_cast<ASTDeRefExpr *>(caseStmt->getCaseExpr()) == nullptr;
    std::vector<StateMap> armStates;
    for (auto *arm : caseStmt->getArms()) {
      armStates.push_back(analyzeArm(arm, byValue, state));
    }
    if (armStates.empty()) {
      return state; // LCOV_EXCL_LINE -- the parser requires at least one arm
    }
    return joinStates(armStates, true);
  }

  // Unknown statement type: no ownership state change.
  return state;
}

// ---------------------------------------------------------------------------
// Case arms: owned binders of a by-value match are arm-scoped owners
// ---------------------------------------------------------------------------

std::vector<ASTDeclNode *>
MoveAnalysis::ownedBinders(ASTCaseArm *arm, bool byValue,
                           OwnershipClassifier *classifier) {
  std::vector<ASTDeclNode *> binders;
  if (!byValue) {
    return binders; // `case *p` binders are aliases, owned by the scrutinee
  }
  for (auto *binding : arm->getBindings()) {
    if (classifier->classify(binding) == OwnershipClass::Own) {
      binders.push_back(binding);
    }
  }
  return binders;
}

MoveAnalysis::StateMap MoveAnalysis::analyzeArm(ASTCaseArm *arm, bool byValue,
                                                StateMap state) {
  // Uses of a binder resolve by name to the function's declaration of that
  // name, so the binder's state is kept under that declaration for the arm and
  // the previous entry (an enclosing binder or local of the same name) is
  // restored when the arm ends.
  std::vector<std::pair<ASTDeclNode *, std::optional<OwnershipState>>> saved;
  for (auto *binder : ownedBinders(arm, byValue, classifier)) {
    ASTDeclNode *key = resolveVar(binder->getName());
    if (key == nullptr) {
      key = binder; // LCOV_EXCL_LINE -- every binding is registered by name
    }
    auto it = state.find(key);
    saved.push_back({key, it != state.end()
                              ? std::optional<OwnershipState>(it->second)
                              : std::nullopt});
    state[key] = OwnershipState::Owned;
    trace.push_back({"own", binder->getName(), arm->getLine(),
                     "ownership received from the matched payload"});
  }
  state = analyzeStmt(arm->getBody(), std::move(state));
  // A binder goes out of scope at the end of its arm; the destruction pass
  // frees it there if it is still Owned. It takes no part in the join.
  for (auto &[key, previous] : saved) {
    if (previous.has_value()) {
      state[key] = *previous;
    } else {
      state.erase(key);
    }
  }
  return state;
}

// ---------------------------------------------------------------------------
// Assignment transfer function
// ---------------------------------------------------------------------------

MoveAnalysis::StateMap MoveAnalysis::analyzeAssign(ASTAssignStmt *stmt,
                                                    StateMap state) {
  ASTExpr *lhs = stmt->getLHS();
  ASTExpr *rhs = stmt->getRHS();

  // Code generation evaluates a `*e = v` target before the right-hand side,
  // so its operand is a use that comes first: a moved owning pointer cannot
  // be written through, and a move in the target is seen by the right side.
  auto *lhsDeref = dynamic_cast<ASTDeRefExpr *>(lhs);
  if (lhsDeref != nullptr) {
    evalExpr(lhsDeref->getPtr(), state);
  }

  auto *rhsVar = dynamic_cast<ASTVariableExpr *>(rhs);
  ASTDeclNode *rhsDecl = rhsVar ? resolveVar(rhsVar->getName()) : nullptr;
  bool rhsIsOwn =
      rhsDecl && classifier->classify(rhsDecl) == OwnershipClass::Own;

  if (rhsIsOwn) {
    // A direct Own variable on the right is a move.
    auto rhsIt = state.find(rhsDecl);
    if (rhsIt != state.end() && rhsIt->second == OwnershipState::Moved) {
      std::ostringstream oss;
      oss << "variable '" << rhsVar->getName()
          << "' moved more than once on line " << stmt->getLine();
      if (RuleToggles::enabled("use-after-move")) throw SemanticError(oss.str());
    }
    state[rhsDecl] = OwnershipState::Moved;
    trace.push_back({"move", rhsVar->getName(), stmt->getLine(),
                     "ownership moved from RHS variable"});
    SEMANTIC_LOG(2, "move-analysis")
      << "line=" << stmt->getLine() << " event=move variable="
      << rhsVar->getName() << " reason=rhs-variable";
  } else {
    evalExpr(rhs, state);
  }

  // Determine if the LHS is a direct Own variable reference.
  auto *lhsVar = dynamic_cast<ASTVariableExpr *>(lhs);
  if (lhsVar == nullptr) {
    return state; // `*e = v`: the target was evaluated above
  }
  ASTDeclNode *lhsDecl = resolveVar(lhsVar->getName());
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
      if (RuleToggles::enabled("assign-over-live")) throw SemanticError(oss.str());
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

// ---------------------------------------------------------------------------
// Expressions, in evaluation order
// ---------------------------------------------------------------------------

void MoveAnalysis::evalExpr(ASTNode *node, StateMap &state) {
  if (node == nullptr) {
    return;
  }

  if (auto *varExpr = dynamic_cast<ASTVariableExpr *>(node)) {
    checkUse(varExpr, state);
    return;
  }

  if (auto *call = dynamic_cast<ASTFunAppExpr *>(node)) {
    evalExpr(call->getFunction(), state);

    // Which actuals this call consumes was decided once, from solved types
    // and callee summaries (FunctionEffectSummaries::callEffect).
    const FunctionEffectSummaries::CallEffect *effect =
        functionEffects != nullptr ? functionEffects->callEffect(call) : nullptr;
    auto actuals = call->getActuals();

    // A borrow in any actual is live until the call returns, so no actual of
    // the same call may consume the borrowed owner.
    std::set<ASTDeclNode *> borrowed;
    for (auto *actual : actuals) {
      collectBorrowedOwners(actual, borrowed);
    }
    heldBorrows.push_back({call, std::move(borrowed)});

    for (std::size_t i = 0; i < actuals.size(); ++i) {
      bool consumes = effect != nullptr && i < effect->consumes.size() &&
                      effect->consumes[i];
      auto *argVar = dynamic_cast<ASTVariableExpr *>(actuals[i]);
      ASTDeclNode *decl = argVar ? resolveVar(argVar->getName()) : nullptr;
      if (consumes && decl != nullptr &&
          classifier->classify(decl) == OwnershipClass::Own) {
        consumeVar(argVar, decl, state,
                   "ownership moved via function argument");
      } else {
        // A temporary (the callee owns it) or a non-consuming actual.
        evalExpr(actuals[i], state);
      }
    }
    heldBorrows.pop_back();
    return;
  }

  // A constructor payload takes ownership of an Own variable: the box owns it
  // from here on, so the variable is moved exactly as if it were passed to a
  // consuming call.
  if (auto *ctor = dynamic_cast<ASTSumCtorExpr *>(node)) {
    for (auto *payload : ctor->getArgs()) {
      auto *payloadVar = dynamic_cast<ASTVariableExpr *>(payload);
      ASTDeclNode *decl =
          payloadVar ? resolveVar(payloadVar->getName()) : nullptr;
      if (decl != nullptr && classifier->classify(decl) == OwnershipClass::Own) {
        consumeVar(payloadVar, decl, state,
                   "ownership moved into constructor payload");
      } else {
        evalExpr(payload, state);
      }
    }
    return;
  }

  for (auto &child : node->getChildren()) {
    evalExpr(child.get(), state);
  }
}

void MoveAnalysis::checkUse(ASTVariableExpr *varExpr,
                            const StateMap &state) const {
  ASTDeclNode *decl = resolveVar(varExpr->getName());
  if (decl && classifier->classify(decl) == OwnershipClass::Own) {
    auto it = state.find(decl);
    if (it != state.end() && it->second == OwnershipState::Moved) {
      std::ostringstream oss;
      oss << "variable '" << varExpr->getName()
          << "' used after move on line " << varExpr->getLine();
      if (RuleToggles::enabled("use-after-move")) throw SemanticError(oss.str());
    }
  }
}

void MoveAnalysis::consumeVar(ASTVariableExpr *varExpr, ASTDeclNode *decl,
                              StateMap &state, const char *reason) {
  auto it = state.find(decl);
  if (it != state.end() && it->second == OwnershipState::Moved) {
    std::ostringstream oss;
    oss << "variable '" << varExpr->getName() << "' "
        << (movedInStmt.count(decl) > 0 ? "moved more than once"
                                        : "used after move")
        << " on line " << varExpr->getLine();
    if (RuleToggles::enabled("use-after-move")) throw SemanticError(oss.str());
  }
  for (const auto &[call, owners] : heldBorrows) {
    if (owners.count(decl) > 0) {
      std::ostringstream oss;
      oss << "Ownership error on line " << varExpr->getLine() << ": variable '"
          << varExpr->getName() << "' is moved while the call " << *call
          << " borrows it; a borrowed owner must stay alive until the call "
             "returns";
      if (RuleToggles::enabled("call-held-borrow")) throw SemanticError(oss.str());
    }
  }
  state[decl] = OwnershipState::Moved;
  movedInStmt.insert(decl);
  trace.push_back({"move", varExpr->getName(), varExpr->getLine(), reason});
  SEMANTIC_LOG(2, "move-analysis")
      << "line=" << varExpr->getLine() << " event=move variable="
      << varExpr->getName() << " reason=" << reason;
}

void MoveAnalysis::collectBorrowedOwners(ASTNode *node,
                                         std::set<ASTDeclNode *> &owners) const {
  if (node == nullptr) {
    return;
  }
  if (auto *borrow = dynamic_cast<ASTBorrowExpr *>(node)) {
    if (auto *var = dynamic_cast<ASTVariableExpr *>(borrow->getVar())) {
      if (auto *decl = resolveVar(var->getName())) {
        owners.insert(decl);
      }
    }
  }
  for (auto &child : node->getChildren()) {
    collectBorrowedOwners(child.get(), owners);
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
// Joins and loops
// ---------------------------------------------------------------------------

MoveAnalysis::StateMap
MoveAnalysis::joinStates(const std::vector<StateMap> &branches, bool check) {
  // A variable is Owned after the join only if it is Owned on every branch.
  // Destruction is decided statically, so it may not be Owned on some
  // branches and not others. Not Owned (never assigned, or Moved) on every
  // branch is fine: there is nothing to free either way.
  std::set<ASTDeclNode *> vars;
  for (const auto &branch : branches) {
    for (const auto &[decl, _] : branch) {
      vars.insert(decl);
    }
  }
  StateMap joined;
  for (auto *decl : vars) {
    std::size_t owned = 0;
    bool moved = false;
    for (const auto &branch : branches) {
      auto it = branch.find(decl);
      if (it == branch.end()) {
        continue;
      }
      if (it->second == OwnershipState::Owned) {
        ++owned;
      } else {
        moved = true;
      }
    }
    if (owned == branches.size()) {
      joined[decl] = OwnershipState::Owned;
      continue;
    }
    if (owned > 0 && check) {
      std::ostringstream oss;
      oss << "ownership state disagreement at control-flow join for "
             "an Own variable: ";
      if (moved) {
        oss << "one path leaves it Moved and the other leaves it Owned";
      } else {
        oss << "variable '" << decl->getName()
            << "' is assigned on one path and not on another; initialize it "
               "on every path";
      }
      if (RuleToggles::enabled("join-agreement")) throw SemanticError(oss.str());
    }
    if (moved) {
      joined[decl] = OwnershipState::Moved;
    }
  }
  return joined;
}

void MoveAnalysis::assertLoopInvariant(const StateMap &preState,
                                       const StateMap &bodyState, int line) {
  auto lookup = [](const StateMap &m, ASTDeclNode *d) {
    auto it = m.find(d);
    return it != m.end() && it->second == OwnershipState::Owned;
  };
  std::set<ASTDeclNode *> vars;
  for (const auto &[decl, _] : preState) vars.insert(decl);
  for (const auto &[decl, _] : bodyState) vars.insert(decl);
  for (auto *decl : vars) {
    bool before = lookup(preState, decl);
    bool after = lookup(bodyState, decl);
    if (before == after) {
      continue;
    }
    std::ostringstream oss;
    if (before) {
      oss << "move inside while-loop body on line " << line;
    } else {
      oss << "variable '" << decl->getName()
          << "' is still owned at the end of a while-loop iteration on line "
          << line << "; move it on or free it within the iteration";
    }
    if (RuleToggles::enabled("loop-body-invariant")) throw SemanticError(oss.str());
  }
}

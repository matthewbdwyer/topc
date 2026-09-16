#include "FunctionEffectSummaries.h"
#include "InternalError.h"
#include "RuleToggles.h"
#include "../SemanticLogging.h"

#include "ASTAssignStmt.h"
#include "ASTAllocExpr.h"
#include "ASTBlockStmt.h"
#include "ASTBorrowExpr.h"
#include "ASTCaseStmt.h"
#include "ASTFunAppExpr.h"
#include "ASTFunction.h"
#include "ASTProgram.h"
#include "ASTVisitor.h"
#include "CallGraph.h"
#include "ASTIfStmt.h"
#include "ASTReturnStmt.h"
#include "ASTVariableExpr.h"
#include "ASTWhileStmt.h"
#include "OwnershipClassifier.h"
#include "OwnershipTypeRules.h"
#include "ReferenceType.h"
#include "SemanticError.h"
#include "SymbolTable.h"
#include "TopFunction.h"
#include "TopAlpha.h"
#include "TopModeVar.h"
#include "TopMu.h"
#include "TopType.h"
#include "TopTypeVisitor.h"
#include "TopVar.h"
#include "TypeInference.h"
#include "TypeVars.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <set>
#include <sstream>

namespace {

const char *formalModeName(FunctionEffectSummaries::FormalMode mode) {
  switch (mode) {
  case FunctionEffectSummaries::FormalMode::Copy: return "Copy";
  case FunctionEffectSummaries::FormalMode::Own: return "Own";
  case FunctionEffectSummaries::FormalMode::DependsOnInstantiation:
    return "DependsOnInstantiation";
  }
  return "Unknown";
}

const char *returnOriginName(FunctionEffectSummaries::ReturnOrigin origin) {
  switch (origin) {
  case FunctionEffectSummaries::ReturnOrigin::Unknown: return "Unknown";
  case FunctionEffectSummaries::ReturnOrigin::PureCopy: return "PureCopy";
  case FunctionEffectSummaries::ReturnOrigin::FreshOwn: return "FreshOwn";
  case FunctionEffectSummaries::ReturnOrigin::FromFormal: return "FromFormal";
  case FunctionEffectSummaries::ReturnOrigin::BorrowFromFormal:
    return "BorrowFromFormal";
  }
  return "Unknown";
}

const char *requirementName(FunctionEffectSummaries::CopyRequirement req) {
  switch (req) {
  case FunctionEffectSummaries::CopyRequirement::None: return "none";
  case FunctionEffectSummaries::CopyRequirement::Referent: return "referent-copy";
  case FunctionEffectSummaries::CopyRequirement::Self: return "self-copy";
  case FunctionEffectSummaries::CopyRequirement::Both: return "referent-and-self-copy";
  }
  return "none"; // LCOV_EXCL_LINE -- unreachable: the switch above is exhaustive
}

FunctionEffectSummaries::CopyRequirement
joinRequirement(FunctionEffectSummaries::CopyRequirement a,
                FunctionEffectSummaries::CopyRequirement b) {
  using R = FunctionEffectSummaries::CopyRequirement;
  if (a == R::None) return b;
  if (b == R::None || a == b) return a;
  return R::Both;
}

/* Collects every call expression in the program with its enclosing function. */
struct CallCollector : public ASTVisitor {
  std::vector<std::pair<ASTFunAppExpr *, ASTDeclNode *>> calls;
  ASTDeclNode *current = nullptr;
  bool visit(ASTFunction *element) override {
    current = element->getDecl();
    return true;
  }
  void endVisit(ASTFunAppExpr *element) override {
    calls.push_back({element, current});
  }
};

struct OriginFact {
  FunctionEffectSummaries::ReturnOrigin origin =
      FunctionEffectSummaries::ReturnOrigin::Unknown;
  int formalIndex = -1;
  bool allowTypeFallback = true;
};

/* A call inside a function body that receives (an alias of) formal `formal`
 * as its actual number `position`. Used to make "passed on" transitive. */
struct ForwardRecord {
  int formal;
  ASTFunAppExpr *call;
  std::size_t position;
};

/* State of the origin walk on one path: where each local's value came from,
 * and which formals have been passed on to a call so far on this path. */
struct OriginState {
  std::map<ASTDeclNode *, OriginFact> origins;
  std::set<int> forwarded;
  /* Formals passed on to a call on *some* path so far, and formals used again
   * after that. A value passed on by value may have been consumed, so a later
   * use is sound only if the value is Copy (a Copy requirement on the formal,
   * judged at each call). */
  std::set<int> maybeForwarded;
  std::set<int> reused;
};

bool sameOrigin(const OriginFact &a, const OriginFact &b) {
  return a.origin == b.origin && a.formalIndex == b.formalIndex &&
         a.allowTypeFallback == b.allowTypeFallback;
}

OriginFact unknownOrigin() { return {}; }

OriginFact conflictOrigin() {
  return {FunctionEffectSummaries::ReturnOrigin::Unknown, -1, false};
}

OriginFact freshOwnOrigin() {
  return {FunctionEffectSummaries::ReturnOrigin::FreshOwn, -1, true};
}

OriginFact fromFormalOrigin(int idx) {
  return {FunctionEffectSummaries::ReturnOrigin::FromFormal, idx, true};
}

OriginFact borrowFromFormalOrigin(int idx) {
  return {FunctionEffectSummaries::ReturnOrigin::BorrowFromFormal, idx, true};
}

OriginFact originForExpr(ASTExpr *expr, const OriginState &state,
                         SymbolTable *sym, ASTDeclNode *functionDecl) {
  if (auto *var = dynamic_cast<ASTVariableExpr *>(expr)) {
    auto *decl = sym->getLocal(var->getName(), functionDecl);
    auto it = state.origins.find(decl);
    if (it != state.origins.end()) {
      return it->second;
    }
    return unknownOrigin();
  }

  if (auto *borrow = dynamic_cast<ASTBorrowExpr *>(expr)) {
    auto *borrowedVar = dynamic_cast<ASTVariableExpr *>(borrow->getVar());
    if (borrowedVar == nullptr) {
      return unknownOrigin();
    }

    auto *decl = sym->getLocal(borrowedVar->getName(), functionDecl);
    auto it = state.origins.find(decl);
    if (it != state.origins.end() &&
        it->second.origin == FunctionEffectSummaries::ReturnOrigin::FromFormal) {
      return borrowFromFormalOrigin(it->second.formalIndex);
    }
  }

  if (dynamic_cast<ASTAllocExpr *>(expr) != nullptr) {
    return freshOwnOrigin();
  }

  return unknownOrigin();
}

/* Walk `node` in evaluation order. Record every call whose actual is (an
 * alias of) a formal: on this path that formal has been passed on. Record a
 * formal as reused when (an alias of) it is used, including under `&`, after
 * it may have been passed on. */
void noteForwards(ASTNode *node, OriginState &state,
                  std::vector<ForwardRecord> &log, SymbolTable *sym,
                  ASTDeclNode *functionDecl) {
  if (node == nullptr) {
    return;
  }
  if (auto *var = dynamic_cast<ASTVariableExpr *>(node)) {
    auto origin = originForExpr(var, state, sym, functionDecl);
    if (origin.origin == FunctionEffectSummaries::ReturnOrigin::FromFormal &&
        state.maybeForwarded.count(origin.formalIndex) > 0) {
      state.reused.insert(origin.formalIndex);
    }
    return;
  }
  if (auto *call = dynamic_cast<ASTFunAppExpr *>(node)) {
    noteForwards(call->getFunction(), state, log, sym, functionDecl);
    auto actuals = call->getActuals();
    for (std::size_t k = 0; k < actuals.size(); ++k) {
      noteForwards(actuals[k], state, log, sym, functionDecl);
      auto origin = originForExpr(actuals[k], state, sym, functionDecl);
      if (origin.origin == FunctionEffectSummaries::ReturnOrigin::FromFormal) {
        state.forwarded.insert(origin.formalIndex);
        state.maybeForwarded.insert(origin.formalIndex);
        log.push_back({origin.formalIndex, call, k});
      }
    }
    return;
  }
  for (auto &child : node->getChildren()) {
    noteForwards(child.get(), state, log, sym, functionDecl);
  }
}

OriginState joinStates(const OriginState &left, const OriginState &right) {
  OriginState joined;
  for (const auto &[decl, leftOrigin] : left.origins) {
    auto it = right.origins.find(decl);
    if (it != right.origins.end() && sameOrigin(leftOrigin, it->second)) {
      joined.origins[decl] = leftOrigin;
    } else {
      joined.origins[decl] = conflictOrigin();
    }
  }

  for (const auto &[decl, rightOrigin] : right.origins) {
    if (left.origins.find(decl) == left.origins.end()) {
      joined.origins[decl] = conflictOrigin();
    }
  }

  // Passed on only if passed on along both paths.
  std::set_intersection(left.forwarded.begin(), left.forwarded.end(),
                        right.forwarded.begin(), right.forwarded.end(),
                        std::inserter(joined.forwarded, joined.forwarded.end()));
  // Maybe passed on, and reused, if so along either path.
  joined.maybeForwarded = left.maybeForwarded;
  joined.maybeForwarded.insert(right.maybeForwarded.begin(),
                               right.maybeForwarded.end());
  joined.reused = left.reused;
  joined.reused.insert(right.reused.begin(), right.reused.end());
  return joined;
}

OriginState analyzeStmtOrigins(ASTStmt *stmt, OriginState state,
                               std::vector<ForwardRecord> &log,
                               SymbolTable *sym, ASTDeclNode *functionDecl);

OriginState analyzeBlockOrigins(const std::vector<ASTStmt *> &stmts,
                                OriginState state,
                                std::vector<ForwardRecord> &log,
                                SymbolTable *sym, ASTDeclNode *functionDecl) {
  for (auto *stmt : stmts) {
    state = analyzeStmtOrigins(stmt, std::move(state), log, sym, functionDecl);
  }
  return state;
}

OriginState analyzeStmtOrigins(ASTStmt *stmt, OriginState state,
                               std::vector<ForwardRecord> &log,
                               SymbolTable *sym, ASTDeclNode *functionDecl) {
  if (auto *assign = dynamic_cast<ASTAssignStmt *>(stmt)) {
    noteForwards(assign->getRHS(), state, log, sym, functionDecl);

    auto *lhsVar = dynamic_cast<ASTVariableExpr *>(assign->getLHS());
    if (lhsVar == nullptr) {
      return state;
    }

    auto *lhsDecl = sym->getLocal(lhsVar->getName(), functionDecl);
    if (lhsDecl == nullptr) {
      return state;
    }

    auto origin = originForExpr(assign->getRHS(), state, sym, functionDecl);
    if (origin.origin == FunctionEffectSummaries::ReturnOrigin::Unknown) {
      state.origins.erase(lhsDecl);
    } else {
      state.origins[lhsDecl] = origin;
    }
    return state;
  }

  if (auto *block = dynamic_cast<ASTBlockStmt *>(stmt)) {
    return analyzeBlockOrigins(block->getStmts(), std::move(state), log, sym,
                               functionDecl);
  }

  if (auto *ifStmt = dynamic_cast<ASTIfStmt *>(stmt)) {
    noteForwards(ifStmt->getCondition(), state, log, sym, functionDecl);
    auto thenState =
        analyzeStmtOrigins(ifStmt->getThen(), state, log, sym, functionDecl);
    auto elseState = ifStmt->getElse() != nullptr
                         ? analyzeStmtOrigins(ifStmt->getElse(), state, log,
                                              sym, functionDecl)
                         : state;
    return joinStates(thenState, elseState);
  }

  if (auto *whileStmt = dynamic_cast<ASTWhileStmt *>(stmt)) {
    // The body may run zero times: what it passes on does not count.
    noteForwards(whileStmt->getCondition(), state, log, sym, functionDecl);
    auto bodyState =
        analyzeStmtOrigins(whileStmt->getBody(), state, log, sym, functionDecl);
    auto joined = joinStates(state, bodyState);
    // A second iteration: a formal passed on in one iteration and used in the
    // condition or body of the next is reused. Only the reuse facts are kept;
    // the log already holds this loop's forwarding records.
    std::vector<ForwardRecord> scratchLog;
    auto again = joined;
    noteForwards(whileStmt->getCondition(), again, scratchLog, sym,
                 functionDecl);
    again = analyzeStmtOrigins(whileStmt->getBody(), again, scratchLog, sym,
                               functionDecl);
    joined.reused.insert(again.reused.begin(), again.reused.end());
    return joined;
  }

  if (auto *caseStmt = dynamic_cast<ASTCaseStmt *>(stmt)) {
    noteForwards(caseStmt->getCaseExpr(), state, log, sym, functionDecl);
    auto arms = caseStmt->getArms();
    if (arms.empty()) {
      return state;
    }

    auto joined = analyzeStmtOrigins(arms[0]->getBody(), state, log, sym,
                                     functionDecl);
    for (std::size_t i = 1; i < arms.size(); ++i) {
      auto armState =
          analyzeStmtOrigins(arms[i]->getBody(), state, log, sym, functionDecl);
      joined = joinStates(joined, armState);
    }
    return joined;
  }

  // Any other statement (output, error, return, ...): its expressions may
  // contain calls that pass a formal on.
  for (auto &child : stmt->getChildren()) {
    noteForwards(child.get(), state, log, sym, functionDecl);
  }
  return state;
}

struct BodyFacts {
  OriginFact returned;
  std::set<int> forwarded;
  std::set<int> reused;
  std::vector<ForwardRecord> log;
};

BodyFacts analyzeBody(ASTFunction *f, SymbolTable *sym) {
  BodyFacts facts;
  OriginState state;
  auto formals = f->getFormals();
  for (std::size_t i = 0; i < formals.size(); ++i) {
    state.origins[formals[i]] = fromFormalOrigin(static_cast<int>(i));
  }

  auto stmts = f->getStmts();
  if (stmts.empty()) {
    facts.returned = unknownOrigin();
    return facts;
  }

  for (std::size_t i = 0; i + 1 < stmts.size(); ++i) {
    state = analyzeStmtOrigins(stmts[i], std::move(state), facts.log, sym,
                               f->getDecl());
  }

  auto *ret = dynamic_cast<ASTReturnStmt *>(stmts.back());
  if (ret == nullptr) {
    facts.returned = unknownOrigin();
  } else {
    noteForwards(ret->getArg(), state, facts.log, sym, f->getDecl());
    facts.returned = originForExpr(ret->getArg(), state, sym, f->getDecl());
  }
  facts.forwarded = state.forwarded;
  facts.reused = state.reused;
  return facts;
}

} // namespace

/* What the first phase (build) leaves for the second (resolveRequirements). */
struct FunctionEffectSummaries::BuildState {
  ASTProgram *ast;
  SymbolTable *sym;
  TypeInference *types;
  CallGraph *cg;
  std::map<ASTDeclNode *, std::vector<ForwardRecord>> forwardLogs;
  std::map<ASTDeclNode *, ASTFunction *> functionsByDecl;
};

std::shared_ptr<FunctionEffectSummaries>
FunctionEffectSummaries::build(ASTProgram *ast, SymbolTable *sym,
                               TypeInference *types,
                               OwnershipClassifier *classifier, CallGraph *cg) {
  SEMANTIC_LOG(1, "function-effects") << "start";
  auto result = std::make_shared<FunctionEffectSummaries>();
  result->buildState = std::make_shared<BuildState>();
  auto &state = *result->buildState;
  state.ast = ast;
  state.sym = sym;
  state.types = types;
  state.cg = cg;
  auto &forwardLogs = state.forwardLogs;
  auto &functionsByDecl = state.functionsByDecl;

  for (auto *f : ast->getFunctions()) {
    Summary summary;
    summary.functionName = f->getName();
    functionsByDecl[f->getDecl()] = f;

    OwnershipTypeRules::rejectUnsupportedRecursiveType(types->getInferredType(f->getDecl()).get(),
                                   "function " + f->getName());

    for (auto *formal : f->getFormals()) {
      summary.formalNames.push_back(formal->getName());
      auto inferred = types->getInferredType(formal);
      OwnershipTypeRules::rejectUnsupportedRecursiveType(inferred.get(),
                                     "parameter " + formal->getName() +
                                         " of function " + f->getName());
      // Own: the callee owns (and frees) the value, even when its type still
      // has variables inside (an owning reference's payload is always Copy).
      // DependsOnInstantiation: the formal's type is not yet known to own
      // anything, so what the call does is decided per call site.
      if (classifier->classify(formal) == OwnershipClass::Own) {
        summary.formalModes.push_back(FormalMode::Own);
      } else if (OwnershipTypeRules::containsTypeVariable(inferred.get())) {
        summary.formalModes.push_back(FormalMode::DependsOnInstantiation);
      } else {
        summary.formalModes.push_back(FormalMode::Copy);
      }
    }

    auto facts = analyzeBody(f, sym);
    summary.formalForwarded.assign(summary.formalModes.size(), false);
    for (int i : facts.forwarded) {
      if (i >= 0 && static_cast<std::size_t>(i) < summary.formalForwarded.size()) {
        summary.formalForwarded[i] = true;
      }
    }
    forwardLogs[f->getDecl()] = std::move(facts.log);

    summary.formalRequirement.assign(summary.formalModes.size(),
                                     CopyRequirement::None);
    summary.formalRequirementReasons.assign(summary.formalModes.size(), 0);
    auto stmts = f->getStmts();
    auto *ret =
        stmts.empty() ? nullptr : dynamic_cast<ASTReturnStmt *>(stmts.back());
    if (ret != nullptr) {
      auto origin = facts.returned;
      summary.returnOrigin = origin.origin;
      summary.returnFormalIndex = origin.formalIndex;

        if (summary.returnOrigin == ReturnOrigin::Unknown &&
          origin.allowTypeFallback) {
        auto fnType =
            std::dynamic_pointer_cast<TopFunction>(types->getInferredType(f->getDecl()));
        auto retType = (fnType != nullptr) ? fnType->getReturnType() : nullptr;
        if (retType != nullptr &&
            OwnershipClassifier::classifyType(retType.get()) == OwnershipClass::Own) {
          summary.returnOrigin = ReturnOrigin::FreshOwn;
        } else {
          summary.returnOrigin = ReturnOrigin::PureCopy;
        }
      }
    }

    // A generic formal used again after it may have been passed on is sound
    // only when its instance is Copy: an owned value passed on was consumed.
    for (int i : facts.reused) {
      if (i >= 0 && static_cast<std::size_t>(i) < summary.formalModes.size() &&
          summary.formalModes[i] == FormalMode::DependsOnInstantiation) {
        summary.formalRequirement[i] =
            joinRequirement(summary.formalRequirement[i], CopyRequirement::Self);
        summary.formalRequirementReasons[i] |= UsedAfterPassedOn;
      }
    }

    // A generic formal must be disposed of on every path, by returning it or
    // passing it on: a body compiled once for every instantiation cannot free
    // a value of variable type. Otherwise its instance must be Copy.
    for (std::size_t i = 0; i < summary.formalModes.size(); ++i) {
      bool returned = summary.returnOrigin == ReturnOrigin::FromFormal &&
                      summary.returnFormalIndex == static_cast<int>(i);
      auto formalType = types->getInferredType(f->getFormals()[i]);
      if (summary.formalModes[i] == FormalMode::DependsOnInstantiation &&
          OwnershipTypeRules::classDependsOnInstantiation(formalType.get()) &&
          !returned && !summary.formalForwarded[i]) {
        summary.formalRequirement[i] =
            joinRequirement(summary.formalRequirement[i], CopyRequirement::Self);
        summary.formalRequirementReasons[i] |= NotDisposed;
      }
    }

    result->summaries[f->getDecl()] = std::move(summary);
  }
  return result;
}

std::vector<FunctionEffectSummaries::Summary *>
FunctionEffectSummaries::targetsOf(ASTFunAppExpr *call) {
  // Possible callees of a call: the named function, else the call graph.
  std::vector<Summary *> targets;
  if (auto *calleeVar = dynamic_cast<ASTVariableExpr *>(call->getFunction())) {
    if (auto *decl = buildState->sym->getFunction(calleeVar->getName())) {
      auto it = summaries.find(decl);
      if (it != summaries.end()) {
        targets.push_back(&it->second);
      }
    }
  }
  if (targets.empty() && buildState->cg != nullptr) {
    for (auto *g : buildState->cg->getCalledFuns(call)) {
      auto it = summaries.find(g->getDecl());
      if (it != summaries.end()) {
        targets.push_back(&it->second);
      }
    }
  }
  return targets;
}

void FunctionEffectSummaries::resolveRequirements(
    const std::map<ASTDeclNode *, std::vector<FormalRequirement>> *requirements) {
  auto *result = this;
  auto *ast = buildState->ast;
  auto *sym = buildState->sym;
  auto *types = buildState->types;
  auto &forwardLogs = buildState->forwardLogs;
  auto &functionsByDecl = buildState->functionsByDecl;

  // Requirements the position check found in generic bodies join the ones the
  // body walk recorded (reuse, not disposed).
  for (auto &[decl, summary] : summaries) {
    if (requirements != nullptr) {
      auto it = requirements->find(decl);
      if (it != requirements->end() &&
          it->second.size() == summary.formalRequirement.size()) {
        for (std::size_t i = 0; i < it->second.size(); ++i) {
          summary.formalRequirement[i] =
              joinRequirement(summary.formalRequirement[i], it->second[i].kind);
          summary.formalRequirementReasons[i] |= it->second[i].reasons;
        }
      }
    }
    SEMANTIC_LOG(2, "function-effects")
        << "function=" << summary.functionName
        << " return-origin=" << returnOriginName(summary.returnOrigin)
        << " return-formal=" << summary.returnFormalIndex;
    for (std::size_t i = 0; i < summary.formalModes.size(); ++i) {
      SEMANTIC_LOG(2, "function-effects")
          << "function=" << summary.functionName << " formal=" << i
          << " mode=" << formalModeName(summary.formalModes[i])
          << " passed-on=" << (summary.formalForwarded[i] ? "yes" : "no")
          << " requires=" << requirementName(summary.formalRequirement[i]);
    }
  }

  // Call-site effects: which actuals each call consumes. Named callees use
  // their summary directly; calls through function values (a formal, a local
  // holding a function) use the call graph's possible targets.
  CallCollector collector;
  ast->accept(&collector);

  // A generic body that takes the value behind a borrowed formal (AliasCheck)
  // is sound only for Copy referents. Decide at each call from the actual's
  // type; where that type is itself still generic, the requirement moves to
  // the enclosing function's formal and the loop runs again.
  enum class Verdict { Satisfied, Violated, Undetermined };
  auto judgeOne = [&](CopyRequirement req, const TopType *actualType) {
    const TopType *subject = actualType;
    if (req == CopyRequirement::Referent) {
      auto *ref = dynamic_cast<const ReferenceType *>(actualType);
      if (ref == nullptr) {
        return OwnershipTypeRules::containsTypeVariable(const_cast<TopType *>(actualType))
                   ? Verdict::Undetermined
                   : Verdict::Satisfied;
      }
      subject = ref->getReferencedType().get();
    }
    if (OwnershipClassifier::classifyType(subject) == OwnershipClass::Own) {
      return Verdict::Violated;
    }
    if (OwnershipTypeRules::classDependsOnInstantiation(subject)) {
      return Verdict::Undetermined;
    }
    return Verdict::Satisfied;
  };
  auto judge = [&](CopyRequirement req, const TopType *actualType) {
    if (req != CopyRequirement::Both) {
      return judgeOne(req, actualType);
    }
    auto a = judgeOne(CopyRequirement::Referent, actualType);
    auto b = judgeOne(CopyRequirement::Self, actualType);
    if (a == Verdict::Violated || b == Verdict::Violated) return Verdict::Violated;
    if (a == Verdict::Undetermined || b == Verdict::Undetermined) {
      return Verdict::Undetermined;
    }
    return Verdict::Satisfied;
  };
  auto formalIndexOf = [&](ASTExpr *expr, ASTDeclNode *scope) -> int {
    auto *var = dynamic_cast<ASTVariableExpr *>(expr);
    if (var == nullptr) {
      return -1;
    }
    auto *decl = sym->getLocal(var->getName(), scope);
    auto formals = functionsByDecl[scope]->getFormals();
    for (std::size_t j = 0; j < formals.size(); ++j) {
      if (formals[j] == decl) {
        return static_cast<int>(j);
      }
    }
    return -1;
  };

  bool changed = true;
  while (changed) {
    changed = false;
    for (auto &[call, scope] : collector.calls) {
      auto actuals = call->getActuals();
      for (const Summary *s : targetsOf(call)) {
        std::size_t n = std::min(actuals.size(), s->formalRequirement.size());
        for (std::size_t i = 0; i < n; ++i) {
          CopyRequirement req = s->formalRequirement[i];
          if (req == CopyRequirement::None) {
            continue;
          }
          auto actualType = types->getInferredType(actuals[i], scope);
          Verdict verdict = judge(req, actualType.get());
          SEMANTIC_LOG(2, "function-effects")
              << "call line=" << call->getLine() << " callee="
              << s->functionName << " formal=" << i << " requires="
              << requirementName(req) << " actual=" << *actualType
              << " verdict="
              << (verdict == Verdict::Satisfied     ? "satisfied"
                  : verdict == Verdict::Violated    ? "violated"
                                                    : "undetermined");
          if (verdict == Verdict::Satisfied) {
            continue;
          }
          if (verdict == Verdict::Violated) {
            unsigned reasons = s->formalRequirementReasons[i];
            // Report the reason that applies to how this actual violates the
            // requirement: an owned value itself (Self) or behind a borrow.
            bool selfViolated =
                judgeOne(CopyRequirement::Self, actualType.get()) ==
                Verdict::Violated;
            std::ostringstream oss;
            oss << "call " << *call << " on line " << call->getLine();
            if (selfViolated && (reasons & NotDisposed) &&
                !(reasons & UsedAfterPassedOn)) {
              std::ostringstream drop;
              drop << "owned value passed to generic formal '"
                   << s->formalNames[i] << "' of '" << s->functionName
                   << "' on line " << call->getLine() << " is neither returned"
                   << (s->returnOrigin == ReturnOrigin::Unknown
                           ? " on every path"
                           : "")
                   << " nor borrowed nor passed on by the callee";
              RuleToggles::reject("generic-drop", drop.str());
              continue;
            }
            if (selfViolated && (reasons & UsedAfterPassedOn)) {
              oss << " passes an owned value to generic formal '"
                  << s->formalNames[i] << "' of '" << s->functionName
                  << "', which uses it again after passing it on";
            } else if (!selfViolated && (reasons & OverwritesThroughBorrow) &&
                       !(reasons & MovesOutOfBorrow)) {
              oss << " overwrites an owned value through the borrow: callee '"
                  << s->functionName << "' writes through formal '"
                  << s->formalNames[i] << "'";
            } else if (selfViolated && (reasons & LendsToMoveOut)) {
              oss << " moves an owned value out of the borrow: callee '"
                  << s->functionName << "' hands a borrow of formal '"
                  << s->formalNames[i]
                  << "' to a callee that takes the value behind it";
            } else {
              oss << " moves an owned value out of the borrow: callee '"
                  << s->functionName << "' dereferences formal '"
                  << s->formalNames[i] << "' in a position that takes ownership";
            }
            RuleToggles::reject("generic-copy-bound", oss.str());
            continue;
          }

          // Undetermined: the enclosing function is generic here too. Its
          // callers must decide, so the requirement moves to its formal.
          CopyRequirement inherited = CopyRequirement::Self;
          unsigned inheritedReasons = s->formalRequirementReasons[i];
          ASTExpr *source = actuals[i];
          if (req != CopyRequirement::Self) {
            if (auto *borrow = dynamic_cast<ASTBorrowExpr *>(source)) {
              source = borrow->getVar(); // &x: x itself must be Copy
              inheritedReasons |= LendsToMoveOut;
            } else {
              inherited = req;
            }
          }
          // Which formal of the enclosing function the actual is: recorded by
          // the body walk (it follows local copies of a formal); for `&x`, the
          // borrowed variable itself.
          int j = -1;
          if (source == actuals[i]) {
            for (const auto &record : forwardLogs[scope]) {
              if (record.call == call && record.position == i) {
                j = record.formal;
              }
            }
          }
          if (j < 0) {
            j = formalIndexOf(source, scope);
          }
          if (j < 0) {
            std::ostringstream oss;
            oss << "call " << *call << " on line " << call->getLine()
                << ": cannot tell whether argument " << i << " ("
                << *actuals[i] << ") refers to an owned value that callee '"
                << s->functionName
                << "' would take; pass the formal parameter itself (or a "
                   "borrow of it) rather than a local copy of it";
            RuleToggles::reject("generic-untracked-actual", oss.str());
            continue;
          }
          Summary &caller = result->summaries[scope];
          CopyRequirement joined =
              joinRequirement(caller.formalRequirement[j], inherited);
          unsigned joinedReasons =
              caller.formalRequirementReasons[j] | inheritedReasons;
          if (joined != caller.formalRequirement[j] ||
              joinedReasons != caller.formalRequirementReasons[j]) {
            caller.formalRequirement[j] = joined;
            caller.formalRequirementReasons[j] = joinedReasons;
            changed = true;
            SEMANTIC_LOG(2, "function-effects")
                << "function=" << caller.functionName << " formal=" << j
                << " inherits requires=" << requirementName(joined)
                << " from call line=" << call->getLine();
          }
        }
      }
    }
  }

  for (auto &[call, scope] : collector.calls) {
    auto targets = targetsOf(call);

    auto actuals = call->getActuals();
    CallEffect effect;
    effect.consumes.assign(actuals.size(), false);
    bool first = true;
    for (const Summary *s : targets) {
      std::vector<bool> consumes(actuals.size(), false);
      std::size_t n = std::min(actuals.size(), s->formalModes.size());
      for (std::size_t i = 0; i < n; ++i) {
        bool actualOwn =
            OwnershipClassifier::classifyType(
                types->getInferredType(actuals[i], scope).get()) ==
            OwnershipClass::Own;
        switch (s->formalModes[i]) {
        case FormalMode::Own:
          consumes[i] = actualOwn;
          break;
        case FormalMode::Copy:
          consumes[i] = false;
          break;
        case FormalMode::DependsOnInstantiation:
          // Consumed when the instance owns. A body that does not dispose of
          // the value carries a Copy requirement, already judged above.
          consumes[i] = actualOwn;
          break;
        }
      }
      if (first) {
        effect.consumes = consumes;
        first = false;
      } else if (consumes != effect.consumes) {
        // Unreachable in TOP: a function value's possible targets are unified to
        // one function type, so their formals classify alike. An internal
        // invariant, not a language rule.
        std::ostringstream oss; // LCOV_EXCL_LINE
        oss << "call on line " << call->getLine() // LCOV_EXCL_LINE
            << " has possible targets that disagree on argument ownership";
        throw InternalError(oss.str()); // LCOV_EXCL_LINE
      }
    }

    std::ostringstream consumed;
    for (std::size_t i = 0; i < effect.consumes.size(); ++i) {
      consumed << (i > 0 ? "," : "") << (effect.consumes[i] ? "1" : "0");
    }
    SEMANTIC_LOG(2, "function-effects")
        << "call line=" << call->getLine() << " targets=" << targets.size()
        << " consumes={" << consumed.str() << "}";
    result->callEffects[call] = std::move(effect);
  }

  SEMANTIC_LOG(1, "function-effects")
      << "complete functions=" << result->summaries.size()
      << " calls=" << result->callEffects.size();
  buildState.reset();
}

const FunctionEffectSummaries::CallEffect *
FunctionEffectSummaries::callEffect(const ASTFunAppExpr *call) const {
  auto it = callEffects.find(call);
  if (it == callEffects.end()) {
    return nullptr;
  }
  return &it->second;
}

const FunctionEffectSummaries::Summary *
FunctionEffectSummaries::get(ASTDeclNode *functionDecl) const {
  auto it = summaries.find(functionDecl);
  if (it == summaries.end()) {
    return nullptr;
  }
  return &it->second;
}

#include "FunctionEffectSummaries.h"
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

bool containsRecursiveFunctionType(TopType *type) {
  if (type == nullptr) {
    return false;
  }

  struct Finder : public TopTypeVisitor {
    bool found = false;
    bool insideRecursiveType = false;

    bool visit(TopMu *) override {
      insideRecursiveType = true;
      return true;
    }

    void endVisit(TopMu *) override { insideRecursiveType = false; }

    bool visit(TopFunction *) override {
      if (insideRecursiveType) {
        found = true;
      }
      return !found;
    }

    bool visit(TopType *) {
      return false;
    }
  } finder;

  type->accept(&finder);
  return finder.found;
}

void rejectUnsupportedRecursiveType(TopType *type, const std::string &context) {
  if (containsRecursiveFunctionType(type)) {
    throw SemanticError("recursive types are not yet supported in ownership analysis: " +
                        context);
  }
}

bool containsModeVariable(const TopType *type) {
  if (type == nullptr) {
    return false;
  }
  if (dynamic_cast<const TopModeVar *>(type) != nullptr) {
    return true;
  }
  if (auto mu = dynamic_cast<const TopMu *>(type)) {
    return containsModeVariable(mu->getT().get());
  }
  for (const auto &child : type->getChildTypes()) {
    if (containsModeVariable(child.get())) {
      return true;
    }
  }
  return false;
}

/* A type still depends on how a call instantiates it when it has a free type
 * variable or an unresolved reference mode. TypeVars::collect ignores the
 * bound variable of a recursive (mu) type, so a concrete recursive sum type
 * does not count. */
bool containsTypeVariable(TopType *type) {
  if (type == nullptr) {
    return false;
  }
  return !TypeVars::collect(type).empty() || containsModeVariable(type);
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

/* Record every call under `node` whose actual is (an alias of) a formal: on
 * this path that formal has been passed on. */
void noteForwards(ASTNode *node, OriginState &state,
                  std::vector<ForwardRecord> &log, SymbolTable *sym,
                  ASTDeclNode *functionDecl) {
  if (node == nullptr) {
    return;
  }
  if (auto *call = dynamic_cast<ASTFunAppExpr *>(node)) {
    auto actuals = call->getActuals();
    for (std::size_t k = 0; k < actuals.size(); ++k) {
      auto origin = originForExpr(actuals[k], state, sym, functionDecl);
      if (origin.origin == FunctionEffectSummaries::ReturnOrigin::FromFormal) {
        state.forwarded.insert(origin.formalIndex);
        log.push_back({origin.formalIndex, call, k});
      }
    }
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
    return joinStates(state, bodyState);
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
  return facts;
}

/* A generic formal that receives an owned value must dispose of it on every
 * path: return it, or pass it on to a call. Otherwise nobody can free it. */
bool formalDropsOwnedValue(const FunctionEffectSummaries::Summary &s,
                           std::size_t i) {
  if (s.formalModes[i] !=
      FunctionEffectSummaries::FormalMode::DependsOnInstantiation) {
    return false;
  }
  bool returned =
      s.returnOrigin == FunctionEffectSummaries::ReturnOrigin::FromFormal &&
      s.returnFormalIndex == static_cast<int>(i);
  return !returned && !s.formalForwarded[i];
}

} // namespace

std::shared_ptr<FunctionEffectSummaries>
FunctionEffectSummaries::build(
    ASTProgram *ast, SymbolTable *sym, TypeInference *types,
    OwnershipClassifier *classifier, CallGraph *cg,
    const std::map<ASTDeclNode *, std::vector<CopyRequirement>> *requirements) {
  SEMANTIC_LOG(1, "function-effects") << "start";
  auto result = std::make_shared<FunctionEffectSummaries>();
  std::map<ASTDeclNode *, std::vector<ForwardRecord>> forwardLogs;
  std::map<ASTDeclNode *, ASTFunction *> functionsByDecl;

  // Possible callees of a call: the named function, else the call graph.
  auto targetsOf = [&](ASTFunAppExpr *call) {
    std::vector<Summary *> targets;
    if (auto *calleeVar = dynamic_cast<ASTVariableExpr *>(call->getFunction())) {
      if (auto *decl = sym->getFunction(calleeVar->getName())) {
        auto it = result->summaries.find(decl);
        if (it != result->summaries.end()) {
          targets.push_back(&it->second);
        }
      }
    }
    if (targets.empty() && cg != nullptr) {
      for (auto *g : cg->getCalledFuns(call)) {
        auto it = result->summaries.find(g->getDecl());
        if (it != result->summaries.end()) {
          targets.push_back(&it->second);
        }
      }
    }
    return targets;
  };

  for (auto *f : ast->getFunctions()) {
    Summary summary;
    summary.functionName = f->getName();
    functionsByDecl[f->getDecl()] = f;

    rejectUnsupportedRecursiveType(types->getInferredType(f->getDecl()).get(),
                                   "function " + f->getName());

    for (auto *formal : f->getFormals()) {
      summary.formalNames.push_back(formal->getName());
      auto inferred = types->getInferredType(formal);
      rejectUnsupportedRecursiveType(inferred.get(),
                                     "parameter " + formal->getName() +
                                         " of function " + f->getName());
      // Own: the callee owns (and frees) the value, even when its type still
      // has variables inside (an owning reference's payload is always Copy).
      // DependsOnInstantiation: the formal's type is not yet known to own
      // anything, so what the call does is decided per call site.
      if (classifier->classify(formal) == OwnershipClass::Own) {
        summary.formalModes.push_back(FormalMode::Own);
      } else if (containsTypeVariable(inferred.get())) {
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
    if (requirements != nullptr) {
      auto it = requirements->find(f->getDecl());
      if (it != requirements->end() &&
          it->second.size() == summary.formalRequirement.size()) {
        summary.formalRequirement = it->second;
      }
    }

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
    result->summaries[f->getDecl()] = std::move(summary);
  }

  // "Passed on" is only a disposal if the receiving formal disposes of the
  // value in turn. Propagate drops backwards through generic callees until
  // nothing changes.
  bool changed = true;
  while (changed) {
    changed = false;
    for (auto &[decl, summary] : result->summaries) {
      for (const auto &record : forwardLogs[decl]) {
        if (!summary.formalForwarded[record.formal]) {
          continue;
        }
        for (Summary *target : targetsOf(record.call)) {
          if (record.position < target->formalModes.size() &&
              formalDropsOwnedValue(*target, record.position)) {
            summary.formalForwarded[record.formal] = false;
            changed = true;
          }
        }
      }
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
        return containsTypeVariable(const_cast<TopType *>(actualType))
                   ? Verdict::Undetermined
                   : Verdict::Satisfied;
      }
      subject = ref->getReferencedType().get();
    }
    if (OwnershipClassifier::classifyType(subject) == OwnershipClass::Own) {
      return Verdict::Violated;
    }
    if (containsTypeVariable(const_cast<TopType *>(subject))) {
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

  changed = true;
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
            std::ostringstream oss;
            oss << "call " << *call << " on line " << call->getLine()
                << " moves an owned value out of the borrow: callee '"
                << s->functionName << "' "
                << (req == CopyRequirement::Self
                        ? "hands a borrow of formal '"
                        : "dereferences formal '")
                << s->formalNames[i]
                << (req == CopyRequirement::Self
                        ? "' to a callee that takes the value behind it"
                        : "' in a position that takes ownership");
            throw SemanticError(oss.str());
          }

          // Undetermined: the enclosing function is generic here too. Its
          // callers must decide, so the requirement moves to its formal.
          CopyRequirement inherited = CopyRequirement::Self;
          ASTExpr *source = actuals[i];
          if (req != CopyRequirement::Self) {
            if (auto *borrow = dynamic_cast<ASTBorrowExpr *>(source)) {
              source = borrow->getVar(); // &x: x itself must be Copy
            } else {
              inherited = req;
            }
          }
          int j = formalIndexOf(source, scope);
          if (j < 0) {
            std::ostringstream oss;
            oss << "call " << *call << " on line " << call->getLine()
                << ": cannot tell whether argument " << i << " ("
                << *actuals[i] << ") refers to an owned value that callee '"
                << s->functionName
                << "' would take; pass a formal parameter or a borrow of one";
            throw SemanticError(oss.str());
          }
          Summary &caller = result->summaries[scope];
          CopyRequirement joined =
              joinRequirement(caller.formalRequirement[j], inherited);
          if (joined != caller.formalRequirement[j]) {
            caller.formalRequirement[j] = joined;
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
        case FormalMode::DependsOnInstantiation: {
          if (!actualOwn) {
            consumes[i] = false;
            break;
          }
          // The callee takes the value, but a generic body cannot free a
          // value of variable type: it must return it or pass it on.
          if (!formalDropsOwnedValue(*s, i)) {
            consumes[i] = true;
            break;
          }
          std::ostringstream oss;
          oss << "owned value passed to generic formal '" << s->formalNames[i]
              << "' of '" << s->functionName << "' on line " << call->getLine()
              << " is neither returned"
              << (s->returnOrigin == ReturnOrigin::Unknown ? " on every path"
                                                           : "")
              << " nor borrowed nor passed on by the callee";
          throw SemanticError(oss.str());
        }
        }
      }
      if (first) {
        effect.consumes = consumes;
        first = false;
      } else if (consumes != effect.consumes) {
        std::ostringstream oss;
        oss << "call on line " << call->getLine()
            << " has possible targets that disagree on argument ownership";
        throw SemanticError(oss.str());
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
  return result;
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

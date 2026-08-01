#include "FunctionEffectSummaries.h"
#include "../SemanticLogging.h"

#include "ASTAssignStmt.h"
#include "ASTAllocExpr.h"
#include "ASTBlockStmt.h"
#include "ASTBorrowExpr.h"
#include "ASTCaseStmt.h"
#include "ASTFunction.h"
#include "ASTIfStmt.h"
#include "ASTReturnStmt.h"
#include "ASTVariableExpr.h"
#include "OwnershipClassifier.h"
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

#include <algorithm>
#include <map>

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

bool containsTypeVariable(const TopType *type) {
  if (type == nullptr) {
    return false;
  }

  if (dynamic_cast<const TopAlpha *>(type) != nullptr ||
      dynamic_cast<const TopModeVar *>(type) != nullptr ||
      dynamic_cast<const TopVar *>(type) != nullptr) {
    return true;
  }

  if (auto mu = dynamic_cast<const TopMu *>(type)) {
    return containsTypeVariable(mu->getT().get());
  }

  for (const auto &child : type->getChildTypes()) {
    if (containsTypeVariable(child.get())) {
      return true;
    }
  }

  return false;
}

struct OriginFact {
  FunctionEffectSummaries::ReturnOrigin origin =
      FunctionEffectSummaries::ReturnOrigin::Unknown;
  int formalIndex = -1;
  bool allowTypeFallback = true;
};

using OriginState = std::map<ASTDeclNode *, OriginFact>;

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
    auto it = state.find(decl);
    if (it != state.end()) {
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
    auto it = state.find(decl);
    if (it != state.end() &&
        it->second.origin == FunctionEffectSummaries::ReturnOrigin::FromFormal) {
      return borrowFromFormalOrigin(it->second.formalIndex);
    }
  }

  if (dynamic_cast<ASTAllocExpr *>(expr) != nullptr) {
    return freshOwnOrigin();
  }

  return unknownOrigin();
}

OriginState joinStates(const OriginState &left, const OriginState &right) {
  OriginState joined;
  for (const auto &[decl, leftOrigin] : left) {
    auto it = right.find(decl);
    if (it != right.end() && sameOrigin(leftOrigin, it->second)) {
      joined[decl] = leftOrigin;
    } else {
      joined[decl] = conflictOrigin();
    }
  }

  for (const auto &[decl, rightOrigin] : right) {
    if (left.find(decl) == left.end()) {
      joined[decl] = conflictOrigin();
    }
  }
  return joined;
}

OriginState analyzeStmtOrigins(ASTStmt *stmt, OriginState state,
                               SymbolTable *sym, ASTDeclNode *functionDecl);

OriginState analyzeBlockOrigins(const std::vector<ASTStmt *> &stmts,
                                OriginState state, SymbolTable *sym,
                                ASTDeclNode *functionDecl) {
  for (auto *stmt : stmts) {
    state = analyzeStmtOrigins(stmt, std::move(state), sym, functionDecl);
  }
  return state;
}

OriginState analyzeStmtOrigins(ASTStmt *stmt, OriginState state,
                               SymbolTable *sym, ASTDeclNode *functionDecl) {
  if (auto *assign = dynamic_cast<ASTAssignStmt *>(stmt)) {
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
      state.erase(lhsDecl);
    } else {
      state[lhsDecl] = origin;
    }
    return state;
  }

  if (auto *block = dynamic_cast<ASTBlockStmt *>(stmt)) {
    return analyzeBlockOrigins(block->getStmts(), std::move(state), sym,
                               functionDecl);
  }

  if (auto *ifStmt = dynamic_cast<ASTIfStmt *>(stmt)) {
    auto thenState =
        analyzeStmtOrigins(ifStmt->getThen(), state, sym, functionDecl);
    auto elseState = ifStmt->getElse() != nullptr
                         ? analyzeStmtOrigins(ifStmt->getElse(), state, sym,
                                              functionDecl)
                         : state;
    return joinStates(thenState, elseState);
  }

  if (auto *caseStmt = dynamic_cast<ASTCaseStmt *>(stmt)) {
    auto arms = caseStmt->getArms();
    if (arms.empty()) {
      return state;
    }

    auto joined = analyzeStmtOrigins(arms[0]->getBody(), state, sym,
                                     functionDecl);
    for (std::size_t i = 1; i < arms.size(); ++i) {
      auto armState =
          analyzeStmtOrigins(arms[i]->getBody(), state, sym, functionDecl);
      joined = joinStates(joined, armState);
    }
    return joined;
  }

  return state;
}

OriginFact computeReturnOrigin(ASTFunction *f, SymbolTable *sym) {
  OriginState state;
  auto formals = f->getFormals();
  for (std::size_t i = 0; i < formals.size(); ++i) {
    state[formals[i]] = fromFormalOrigin(static_cast<int>(i));
  }

  auto stmts = f->getStmts();
  if (stmts.empty()) {
    return unknownOrigin();
  }

  for (std::size_t i = 0; i + 1 < stmts.size(); ++i) {
    state = analyzeStmtOrigins(stmts[i], std::move(state), sym, f->getDecl());
  }

  auto *ret = dynamic_cast<ASTReturnStmt *>(stmts.back());
  if (ret == nullptr) {
    return unknownOrigin();
  }

  return originForExpr(ret->getArg(), state, sym, f->getDecl());
}

} // namespace

std::shared_ptr<FunctionEffectSummaries>
FunctionEffectSummaries::build(ASTProgram *ast, SymbolTable *sym,
                               TypeInference *types,
                               OwnershipClassifier *classifier) {
  SEMANTIC_LOG(1, "function-effects") << "start";
  auto result = std::make_shared<FunctionEffectSummaries>();

  for (auto *f : ast->getFunctions()) {
    Summary summary;
    summary.functionName = f->getName();

    rejectUnsupportedRecursiveType(types->getInferredType(f->getDecl()).get(),
                                   "function " + f->getName());

    for (auto *formal : f->getFormals()) {
      auto inferred = types->getInferredType(formal);
      rejectUnsupportedRecursiveType(inferred.get(),
                                     "parameter " + formal->getName() +
                                         " of function " + f->getName());
      if (containsTypeVariable(inferred.get())) {
        summary.formalModes.push_back(FormalMode::DependsOnInstantiation);
      } else if (classifier->classify(formal) == OwnershipClass::Own) {
        summary.formalModes.push_back(FormalMode::Own);
      } else {
        summary.formalModes.push_back(FormalMode::Copy);
      }
    }

    auto stmts = f->getStmts();
    auto *ret =
        stmts.empty() ? nullptr : dynamic_cast<ASTReturnStmt *>(stmts.back());
    if (ret != nullptr) {
      auto origin = computeReturnOrigin(f, sym);
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
          << " mode=" << formalModeName(summary.formalModes[i]);
    }
    result->summaries[f->getDecl()] = std::move(summary);
  }

  SEMANTIC_LOG(1, "function-effects")
      << "complete functions=" << result->summaries.size();
  return result;
}

const FunctionEffectSummaries::Summary *
FunctionEffectSummaries::get(ASTDeclNode *functionDecl) const {
  auto it = summaries.find(functionDecl);
  if (it == summaries.end()) {
    return nullptr;
  }
  return &it->second;
}

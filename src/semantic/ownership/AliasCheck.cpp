#include "AliasCheck.h"
#include "RuleToggles.h"
#include "../SemanticLogging.h"

#include "ASTAssignStmt.h"
#include "ASTBlockStmt.h"
#include "ASTBorrowExpr.h"
#include "ASTCaseArm.h"
#include "ASTCaseStmt.h"
#include "ASTDeRefExpr.h"
#include "ASTErrorStmt.h"
#include "ASTFunction.h"
#include "ASTIfStmt.h"
#include "ASTOutputStmt.h"
#include "ASTProgram.h"
#include "ASTReturnStmt.h"
#include "ASTVariableExpr.h"
#include "ASTWhileStmt.h"
#include "OwnershipClassifier.h"
#include "ReferenceMode.h"
#include "ReferenceType.h"
#include "SemanticError.h"
#include "SymbolTable.h"
#include "TopVar.h"
#include "TypeInference.h"

#include <sstream>

namespace {

std::string repr(const ASTNode *node) {
  std::ostringstream oss;
  oss << *node;
  return oss.str();
}

} // namespace

AliasCheck::Requirements AliasCheck::run(ASTProgram *p, SymbolTable *sym,
                                         TypeInference *types,
                                         OwnershipClassifier *classifier) {
  SEMANTIC_LOG(1, "alias-check") << "start";
  AliasCheck checker(sym, types, classifier);
  for (auto *f : p->getFunctions()) {
    checker.checkFunction(f);
  }
  SEMANTIC_LOG(1, "alias-check")
      << "complete functions=" << checker.requirements.size();
  return std::move(checker.requirements);
}

void AliasCheck::checkFunction(ASTFunction *f) {
  current = f;
  aliasNames.clear();
  requirements[f->getDecl()].assign(f->getFormals().size(),
                                    FormalRequirement{});
  for (auto *stmt : f->getStmts()) {
    checkStmt(stmt);
  }
}

void AliasCheck::checkStmt(ASTStmt *stmt) {
  if (auto *assign = dynamic_cast<ASTAssignStmt *>(stmt)) {
    if (auto *target = dynamic_cast<ASTDeRefExpr *>(assign->getLHS())) {
      // Write-through: the *slot* is taken over, so it must be Copy.
      checkDerefTaken(target, "overwrite");
      checkExpr(target->getPtr(), Ctx::Deref);
    } else {
      checkExpr(assign->getLHS(), Ctx::Move);
    }
    checkExpr(assign->getRHS(), Ctx::Move);
    return;
  }
  if (auto *caseStmt = dynamic_cast<ASTCaseStmt *>(stmt)) {
    // `case *e` borrows; `case v` on an alias binder would consume it.
    auto *scrutinee = caseStmt->getCaseExpr();
    bool borrowed = dynamic_cast<ASTDeRefExpr *>(scrutinee) != nullptr;
    checkExpr(scrutinee, borrowed ? Ctx::Scrutinee : Ctx::Move);
    for (auto *arm : caseStmt->getArms()) {
      // Every binding of the arm shadows an enclosing binding of the same name
      // for the arm's body. Under a borrowed scrutinee an Own-typed binding
      // names a payload the caller still owns: an alias. Sum payloads are
      // monomorphic, so this is decidable here. Binders are tracked by name
      // and arm, not by declaration, because uses resolve by name and the
      // symbol table holds one declaration per name.
      auto saved = aliasNames;
      for (auto *binding : arm->getBindings()) {
        auto type = types->getInferredType(binding);
        if (borrowed && OwnershipClassifier::classifyType(type.get()) ==
                            OwnershipClass::Own) {
          aliasNames.insert(binding->getName());
          SEMANTIC_LOG(2, "alias-check")
              << "function=" << current->getName()
              << " alias-binder=" << binding->getName()
              << " line=" << caseStmt->getLine();
        } else {
          aliasNames.erase(binding->getName());
        }
      }
      checkStmt(arm->getBody());
      aliasNames = saved;
    }
    return;
  }
  if (auto *block = dynamic_cast<ASTBlockStmt *>(stmt)) {
    for (auto *s : block->getStmts()) {
      checkStmt(s);
    }
    return;
  }
  if (auto *ifStmt = dynamic_cast<ASTIfStmt *>(stmt)) {
    checkExpr(ifStmt->getCondition(), Ctx::Move);
    checkStmt(ifStmt->getThen());
    if (ifStmt->getElse() != nullptr) {
      checkStmt(ifStmt->getElse());
    }
    return;
  }
  if (auto *whileStmt = dynamic_cast<ASTWhileStmt *>(stmt)) {
    checkExpr(whileStmt->getCondition(), Ctx::Move);
    checkStmt(whileStmt->getBody());
    return;
  }
  // return, output, error, and anything else: every expression is taken.
  for (auto &child : stmt->getChildren()) {
    if (auto *e = dynamic_cast<ASTExpr *>(child.get())) {
      checkExpr(e, Ctx::Move);
    } else if (auto *s = dynamic_cast<ASTStmt *>(child.get())) {
      checkStmt(s); // LCOV_EXCL_LINE -- defensive: no other statement nests statements
    }
  }
}

void AliasCheck::checkExpr(ASTExpr *expr, Ctx ctx) {
  if (expr == nullptr) {
    return; // LCOV_EXCL_LINE -- defensive: children are never null
  }

  if (auto *var = dynamic_cast<ASTVariableExpr *>(expr)) {
    if (ctx == Ctx::Move && aliasNames.count(var->getName()) > 0) {
      std::ostringstream oss;
      oss << "Ownership error on line " << var->getLine() << ": '"
          << var->getName()
          << "' is bound by matching a borrowed value and can only be "
             "reborrowed (&"
          << var->getName() << ").";
      if (RuleToggles::enabled("alias-binder")) throw SemanticError(oss.str());
    }
    return;
  }

  if (auto *deref = dynamic_cast<ASTDeRefExpr *>(expr)) {
    if (ctx == Ctx::Move) {
      checkDerefTaken(deref, "move");
    }
    checkExpr(deref->getPtr(), Ctx::Deref);
    return;
  }

  if (auto *borrow = dynamic_cast<ASTBorrowExpr *>(expr)) {
    checkExpr(borrow->getVar(), Ctx::Reborrow);
    return;
  }

  // Calls, constructors, arithmetic, alloc: every operand is taken.
  for (auto &child : expr->getChildren()) {
    if (auto *e = dynamic_cast<ASTExpr *>(child.get())) {
      checkExpr(e, Ctx::Move);
    }
  }
}

// `*e` in a position that takes the value (or, for `what == "overwrite"`, the
// slot `*e` being replaced). Concretely Own: reject here. Still a type
// variable: the caller decides, so record the requirement on the formal.
void AliasCheck::checkDerefTaken(ASTDeRefExpr *deref, const char *what) {
  auto type = types->getInferredType(deref);
  if (dynamic_cast<const TopVar *>(type.get()) != nullptr) {
    // Through an *owning* reference the referent can never be Own (alloc
    // payloads are Copy), so `**p` reads a Copy whatever a turns out to be.
    auto ptrType = std::dynamic_pointer_cast<ReferenceType>(
        types->getInferredType(deref->getPtr(), current->getDecl()));
    auto mode = ptrType != nullptr
                    ? std::dynamic_pointer_cast<ReferenceMode>(ptrType->getMode())
                    : nullptr;
    if (mode != nullptr && mode->getMode() == ReferenceMode::Mode::Own) {
      return;
    }
    require(deref->getPtr(), deref, what);
    return;
  }
  if (OwnershipClassifier::classifyType(type.get()) != OwnershipClass::Own) {
    return;
  }
  std::ostringstream oss;
  oss << "Ownership error on line " << deref->getLine() << ": ";
  if (std::string(what) == "overwrite") {
    oss << "cannot overwrite the owned value '" << repr(deref)
        << "' through a borrow.";
  } else {
    oss << "'" << repr(deref)
        << "' is an owned value reached through a borrow and cannot be moved "
           "out; reborrow it with & or build a copy.";
  }
  if (RuleToggles::enabled(std::string(what) == "overwrite" ? "alias-overwrite"
                                                            : "alias-move-out")) {
    throw SemanticError(oss.str());
  }
}

void AliasCheck::require(ASTExpr *operand, ASTDeRefExpr *deref,
                         const char *what) {
  auto *var = dynamic_cast<ASTVariableExpr *>(operand);
  auto *decl = var != nullptr
                   ? sym->getLocal(var->getName(), current->getDecl())
                   : nullptr;
  auto formals = current->getFormals();
  for (std::size_t i = 0; i < formals.size(); ++i) {
    if (formals[i] == decl) {
      auto &requirement = requirements[current->getDecl()][i];
      requirement.kind = Requirement::Referent;
      requirement.reasons |=
          std::string(what) == "overwrite"
              ? FunctionEffectSummaries::OverwritesThroughBorrow
              : FunctionEffectSummaries::MovesOutOfBorrow;
      SEMANTIC_LOG(2, "alias-check")
          << "function=" << current->getName() << " formal=" << i
          << " requires=referent-copy reason=" << what << " line="
          << deref->getLine();
      return;
    }
  }
  // Not a formal: nobody can decide this later, so decide now.
  std::ostringstream oss;
  oss << "Ownership error on line " << deref->getLine()
      << ": cannot tell whether '" << repr(deref)
      << "' is an owned value; dereference a formal parameter directly so the "
         "decision can be made where the function is called.";
  if (RuleToggles::enabled("alias-undecidable")) throw SemanticError(oss.str());
}

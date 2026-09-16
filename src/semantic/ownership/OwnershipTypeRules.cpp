#include "OwnershipTypeRules.h"
#include "RuleToggles.h"
#include "../SemanticLogging.h"

#include "ASTAllocExpr.h"
#include "ASTFunction.h"
#include "ASTProgram.h"
#include "ASTSumTypeDecl.h"
#include "ASTSumVariant.h"
#include "ASTVisitor.h"
#include "OwnershipClassifier.h"
#include "ReferenceMode.h"
#include "ReferenceType.h"
#include "SymbolTable.h"
#include "TopFunction.h"
#include "TopModeVar.h"
#include "TopMu.h"
#include "TopOwningRef.h"
#include "TopTypeVisitor.h"
#include "TopVar.h"
#include "TypeInference.h"
#include "TypeVars.h"

#include <functional>
#include <set>
#include <sstream>

namespace {

const char *kComponentAdvice =
    "; a borrow cannot be stored inside a value — pass it as an immediate call "
    "argument or store an owned value or a copy instead.";

bool containsBorrow(const TopType *type, std::set<const TopType *> &visited) {
  if (type == nullptr || !visited.insert(type).second) {
    return false;
  }
  if (auto *ref = dynamic_cast<const ReferenceType *>(type)) {
    auto mode = std::dynamic_pointer_cast<ReferenceMode>(ref->getMode());
    if (mode != nullptr && mode->getMode() == ReferenceMode::Mode::Borrow) {
      return true;
    }
    return containsBorrow(ref->getReferencedType().get(), visited);
  }
  if (auto *mu = dynamic_cast<const TopMu *>(type)) {
    return containsBorrow(mu->getT().get(), visited);
  }
  // A stored function value owns nothing; its parameter modes are its own
  // business. Type variables cannot yet contain anything.
  if (dynamic_cast<const TopFunction *>(type) != nullptr ||
      dynamic_cast<const TopVar *>(type) != nullptr) {
    return false;
  }
  for (const auto &child : type->getChildTypes()) {
    if (containsBorrow(child.get(), visited)) {
      return true;
    }
  }
  return false;
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
  } finder;

  type->accept(&finder);
  return finder.found;
}

/* Calls \p fn on every `alloc` expression in the program. */
void forEachAlloc(ASTProgram *p, const std::function<void(ASTAllocExpr *)> &fn) {
  struct Visitor : public ASTVisitor {
    const std::function<void(ASTAllocExpr *)> &fn;
    explicit Visitor(const std::function<void(ASTAllocExpr *)> &fn) : fn(fn) {}
    void endVisit(ASTAllocExpr *element) override { fn(element); }
  } visitor(fn);
  p->accept(&visitor);
}

} // namespace

bool OwnershipTypeRules::containsBorrow(const TopType *type) {
  std::set<const TopType *> visited;
  return ::containsBorrow(type, visited);
}

bool OwnershipTypeRules::containsTypeVariable(TopType *type) {
  if (type == nullptr) {
    return false;
  }
  return !TypeVars::collect(type).empty() || containsModeVariable(type);
}

bool OwnershipTypeRules::classDependsOnInstantiation(const TopType *type) {
  if (dynamic_cast<const TopVar *>(type) != nullptr) {
    return true;
  }
  if (auto *ref = dynamic_cast<const ReferenceType *>(type)) {
    return dynamic_cast<const TopModeVar *>(ref->getMode().get()) != nullptr;
  }
  return false;
}

void OwnershipTypeRules::rejectUnsupportedRecursiveType(
    TopType *type, const std::string &context) {
  if (containsRecursiveFunctionType(type)) {
    RuleToggles::reject(
        "recursive-type",
        "recursive types are not yet supported in ownership analysis: " +
            context);
  }
}

void OwnershipTypeRules::checkAllocPayloads(ASTProgram *p,
                                            TypeInference *types) {
  SEMANTIC_LOG(1, "alloc-payload") << "start";
  forEachAlloc(p, [&](ASTAllocExpr *element) {
    // The solved type of `alloc E` is `own&T`; classify the payload `T`.
    auto owningRef =
        std::dynamic_pointer_cast<TopOwningRef>(types->getInferredType(element));
    if (owningRef == nullptr) {
      // After inference an alloc is always an owning reference; be defensive
      // and leave anything unexpected to the other passes.
      return; // LCOV_EXCL_LINE
    }
    auto payload = owningRef->getReferencedType();
    if (OwnershipClassifier::classifyType(payload.get()) != OwnershipClass::Own) {
      return;
    }
    std::ostringstream oss;
    oss << "Ownership error on line " << element->getLine()
        << ": alloc payload must not be an owned value; owned pointers cannot "
           "nest (own&own is not allowed). Use a sum type to own structured or "
           "heap data.\n";
    RuleToggles::reject("alloc-payload", oss.str());
  });
  SEMANTIC_LOG(1, "alloc-payload") << "complete";
}

void OwnershipTypeRules::checkBorrowComponents(ASTProgram *p, SymbolTable *sym,
                                               TypeInference *types) {
  SEMANTIC_LOG(1, "borrow-components") << "start";

  // Sum-type payloads.
  for (const auto &name : sym->getSumTypes()) {
    auto *decl = sym->getSumType(name);
    for (auto *variant : decl->getVariants()) {
      for (auto *param : variant->getParams()) {
        auto payload = types->getInferredType(param);
        if (!containsBorrow(payload.get())) {
          continue;
        }
        std::ostringstream oss;
        oss << "Ownership error on line " << decl->getLine() << ": payload '"
            << param->getName() << "' of constructor " << variant->getTag()
            << " holds a borrow (" << *payload << ")" << kComponentAdvice;
        RuleToggles::reject("borrow-component", oss.str());
      }
    }
  }

  // Function results.
  for (auto *decl : sym->getFunctions()) {
    auto fnType =
        std::dynamic_pointer_cast<TopFunction>(types->getInferredType(decl));
    if (fnType == nullptr) {
      continue; // LCOV_EXCL_LINE -- defensive: every function solves to a function type
    }
    auto ret = fnType->getReturnType();
    if (!containsBorrow(ret.get())) {
      continue;
    }
    // LCOV_EXCL_START -- defensive: a function whose return type is concretely
    // a borrow derives it from a borrow-returning expression, which the flow
    // checker (escapes into return) rejects first. Kept so the invariant does
    // not depend on pass order.
    std::ostringstream oss;
    oss << "Ownership error on line " << decl->getLine() << ": function "
        << decl->getName() << " returns a borrow (" << *ret << ")"
        << kComponentAdvice;
    RuleToggles::reject("borrow-component", oss.str());
    // LCOV_EXCL_STOP
  }

  // Alloc payloads.
  forEachAlloc(p, [&](ASTAllocExpr *element) {
    auto ref =
        std::dynamic_pointer_cast<ReferenceType>(types->getInferredType(element));
    if (ref == nullptr) {
      return; // LCOV_EXCL_LINE -- defensive: an alloc always solves to a reference
    }
    auto payload = ref->getReferencedType();
    if (!containsBorrow(payload.get())) {
      return;
    }
    std::ostringstream oss;
    oss << "Ownership error on line " << element->getLine()
        << ": alloc payload holds a borrow (" << *payload << ")"
        << kComponentAdvice;
    RuleToggles::reject("borrow-component", oss.str());
  });

  SEMANTIC_LOG(1, "borrow-components") << "complete";
}

#include "CheckBorrowComponents.h"
#include "../SemanticLogging.h"

#include "ASTAllocExpr.h"
#include "ASTFunction.h"
#include "ASTProgram.h"
#include "ASTSumTypeDecl.h"
#include "ASTSumVariant.h"
#include "ReferenceMode.h"
#include "ReferenceType.h"
#include "SemanticError.h"
#include "SymbolTable.h"
#include "TopFunction.h"
#include "TopMu.h"
#include "TopVar.h"
#include "TypeInference.h"

#include <sstream>

namespace {

const char *kAdvice =
    "; a borrow cannot be stored inside a value — pass it as an immediate call "
    "argument or store an owned value or a copy instead.";

} // namespace

bool CheckBorrowComponents::containsBorrow(const TopType *type) {
  std::set<const TopType *> visited;
  return containsBorrow(type, visited);
}

bool CheckBorrowComponents::containsBorrow(const TopType *type,
                                           std::set<const TopType *> &visited) {
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

void CheckBorrowComponents::checkSumTypes() {
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
            << " holds a borrow (" << *payload << ")" << kAdvice;
        throw SemanticError(oss.str());
      }
    }
  }
}

void CheckBorrowComponents::checkFunctionReturns() {
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
        << decl->getName() << " returns a borrow (" << *ret << ")" << kAdvice;
    throw SemanticError(oss.str());
    // LCOV_EXCL_STOP
  }
}

void CheckBorrowComponents::endVisit(ASTAllocExpr *element) {
  auto allocType = types->getInferredType(element);
  auto ref = std::dynamic_pointer_cast<ReferenceType>(allocType);
  if (ref == nullptr) {
    return; // LCOV_EXCL_LINE -- defensive: an alloc always solves to a reference
  }
  auto payload = ref->getReferencedType();
  if (!containsBorrow(payload.get())) {
    return;
  }
  std::ostringstream oss;
  oss << "Ownership error on line " << element->getLine()
      << ": alloc payload holds a borrow (" << *payload << ")" << kAdvice;
  throw SemanticError(oss.str());
}

void CheckBorrowComponents::check(ASTProgram *p, SymbolTable *sym,
                                  TypeInference *types) {
  SEMANTIC_LOG(1, "borrow-components") << "start";
  CheckBorrowComponents checker(sym, types);
  checker.checkSumTypes();
  checker.checkFunctionReturns();
  p->accept(&checker);
  SEMANTIC_LOG(1, "borrow-components") << "complete";
}

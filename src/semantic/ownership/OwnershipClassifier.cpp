#include "OwnershipClassifier.h"
#include "../SemanticLogging.h"

#include "InternalError.h"
#include "ReferenceMode.h"
#include "ReferenceType.h"
#include "TopAlpha.h"
#include "TopBorrowRef.h"
#include "TopCons.h"
#include "TopFunction.h"
#include "TopInt.h"
#include "TopModeVar.h"
#include "TopMu.h"
#include "TopOwningRef.h"
#include "TopSumType.h"
#include "TopVar.h"

OwnershipClassifier::OwnershipClassifier(SymbolTable *symTable,
                                         TypeInference *typeInf) {
  SEMANTIC_LOG(1, "ownership-classification") << "start";
  std::size_t declarationCount = 0;
  // Classify every function declaration node.
  for (auto *funcDecl : symTable->getFunctions()) {
    auto type = typeInf->getInferredType(funcDecl);
    classes[funcDecl] = classifyType(type.get());
    ++declarationCount;
    SEMANTIC_LOG(2, "ownership-classification")
        << "declaration=" << funcDecl->getName() << " type=" << *type
        << " class="
        << (classes[funcDecl] == OwnershipClass::Own ? "Own" : "Copy");

    // Classify every local variable (parameters + var declarations) within
    // this function.
    for (auto *localDecl : symTable->getLocals(funcDecl)) {
      auto localType = typeInf->getInferredType(localDecl);
      classes[localDecl] = classifyType(localType.get());
      ++declarationCount;
      SEMANTIC_LOG(2, "ownership-classification")
          << "declaration=" << funcDecl->getName() << "."
          << localDecl->getName() << " type=" << *localType << " class="
          << (classes[localDecl] == OwnershipClass::Own ? "Own" : "Copy");
    }
  }
  SEMANTIC_LOG(1, "ownership-classification")
      << "complete declarations=" << declarationCount;
}

OwnershipClass OwnershipClassifier::classify(ASTDeclNode *node) const {
  auto it = classes.find(node);
  if (it != classes.end()) {
    return it->second;
  }
  // Unregistered node: conservatively Copy.
  return OwnershipClass::Copy;
}

OwnershipClass OwnershipClassifier::classifyType(const TopType *type) {
  if (type == nullptr) {
    return OwnershipClass::Copy;
  }

  // TopOwningRef → Own (owns a heap resource)
  if (dynamic_cast<const TopOwningRef *>(type) != nullptr) {
    return OwnershipClass::Own;
  }

  // TopBorrowRef → Copy (a borrow is not an owner)
  if (dynamic_cast<const TopBorrowRef *>(type) != nullptr) {
    return OwnershipClass::Copy;
  }

  if (auto *reference = dynamic_cast<const ReferenceType *>(type)) {
    if (auto mode =
            std::dynamic_pointer_cast<ReferenceMode>(reference->getMode())) {
      return mode->getMode() == ReferenceMode::Mode::Own
                 ? OwnershipClass::Own
                 : OwnershipClass::Copy;
    }
    if (std::dynamic_pointer_cast<TopModeVar>(reference->getMode()) != nullptr) {
      return OwnershipClass::Copy;
    }
    throw InternalError("reference has unsupported ownership mode");
  }

  // TopInt → Copy
  if (dynamic_cast<const TopInt *>(type) != nullptr) {
    return OwnershipClass::Copy;
  }

  // TopFunction → Copy (functions are not heap resources in v1)
  if (dynamic_cast<const TopFunction *>(type) != nullptr) {
    return OwnershipClass::Copy;
  }

  if (auto *mu = dynamic_cast<const TopMu *>(type)) {
    return classifyType(mu->getT().get());
  }

  // TopSumType → Own: a constructor value is always heap-boxed (calloc), so the
  // box is an owned resource that must be freed, regardless of payload class.
  if (dynamic_cast<const TopSumType *>(type) != nullptr) {
    return OwnershipClass::Own;
  }

  // TopAlpha / TopVar (unresolved type variable) → Copy
  if (dynamic_cast<const TopVar *>(type) != nullptr) {
    return OwnershipClass::Copy;
  }

  throw InternalError("unsupported type in ownership classification: " +
                      type->toString());
}

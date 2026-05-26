#include "OwnershipClassifier.h"

#include "TopAlpha.h"
#include "TopBorrowRef.h"
#include "TopCons.h"
#include "TopFunction.h"
#include "TopInt.h"
#include "TopOwningRef.h"
#include "TopRef.h"
#include "TopSumType.h"
#include "TopVar.h"

OwnershipClassifier::OwnershipClassifier(SymbolTable *symTable,
                                         TypeInference *typeInf) {
  // Classify every function declaration node.
  for (auto *funcDecl : symTable->getFunctions()) {
    auto type = typeInf->getInferredType(funcDecl);
    classes[funcDecl] = classifyType(type.get());

    // Classify every local variable (parameters + var declarations) within
    // this function.
    for (auto *localDecl : symTable->getLocals(funcDecl)) {
      auto localType = typeInf->getInferredType(localDecl);
      classes[localDecl] = classifyType(localType.get());
    }
  }
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

  // TopInt → Copy
  if (dynamic_cast<const TopInt *>(type) != nullptr) {
    return OwnershipClass::Copy;
  }

  // TopFunction → Copy (functions are not heap resources in v1)
  if (dynamic_cast<const TopFunction *>(type) != nullptr) {
    return OwnershipClass::Copy;
  }

  // TopRef (legacy raw pointer) → Copy
  if (dynamic_cast<const TopRef *>(type) != nullptr) {
    return OwnershipClass::Copy;
  }

  // TopSumType → Own if any constructor payload is Own, else Copy
  if (dynamic_cast<const TopSumType *>(type) != nullptr) {
    auto *sum = dynamic_cast<const TopSumType *>(type);
    for (auto &payload : sum->getArguments()) {
      if (classifyType(payload.get()) == OwnershipClass::Own) {
        return OwnershipClass::Own;
      }
    }
    return OwnershipClass::Copy;
  }

  // TopAlpha / TopVar (unresolved type variable) → Copy
  if (dynamic_cast<const TopVar *>(type) != nullptr) {
    return OwnershipClass::Copy;
  }

  // Any other type → Copy (safe default)
  return OwnershipClass::Copy;
}

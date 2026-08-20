#include "CheckAllocPayload.h"
#include "../SemanticLogging.h"
#include "ASTAllocExpr.h"
#include "ASTProgram.h"
#include "OwnershipClassifier.h"
#include "SemanticError.h"
#include "TopOwningRef.h"
#include "TypeInference.h"

#include <sstream>

void CheckAllocPayload::endVisit(ASTAllocExpr *element) {
  // The solved type of `alloc E` is `own&T`; classify the payload `T`.
  auto allocType = typeInf->getInferredType(element);
  auto owningRef = std::dynamic_pointer_cast<TopOwningRef>(allocType);
  if (owningRef == nullptr) {
    // After inference an alloc is always an owning reference; be defensive and
    // leave anything unexpected to the other passes.
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
  throw SemanticError(oss.str());
}

void CheckAllocPayload::check(ASTProgram *p, TypeInference *typeInf) {
  SEMANTIC_LOG(1, "alloc-payload") << "start";
  CheckAllocPayload visitor(typeInf);
  p->accept(&visitor);
  SEMANTIC_LOG(1, "alloc-payload") << "complete";
}

#pragma once

#include "ASTVisitor.h"

class ASTProgram;
class ASTAllocExpr;
class TypeInference;

/*! \class CheckAllocPayload
 *  \brief Reject `alloc E` whose payload is itself an owned value.
 *
 * TOP guarantees that every owned value is freed exactly once.  An owning
 * pointer `own&T` is only well-formed when its payload `T` is a non-owning
 * (Copy) type.  Allowing `own&own&T` -- an owned pointer to an owned value --
 * would introduce a nested owner whose inner resource cannot be reliably
 * freed, reopening a leak.  Structured or heap-owned data is instead expressed
 * with a sum type, whose payloads are freed recursively.
 *
 * This check runs after type inference: it reads the fully-solved type of each
 * `alloc` expression (which is `own&T`) and classifies its payload `T`.  If the
 * payload is owning, the program is rejected.  Because it uses the inferred
 * type rather than the syntactic operand, it catches every owned payload --
 * including `alloc f()` where `f` returns an owned pointer, and `alloc x`
 * where `x` is an owned variable -- not just literally nested `alloc` forms.
 */
class CheckAllocPayload : public ASTVisitor {
public:
  explicit CheckAllocPayload(TypeInference *typeInf) : typeInf(typeInf) {}
  static void check(ASTProgram *p, TypeInference *typeInf);
  void endVisit(ASTAllocExpr *element) override;

private:
  TypeInference *typeInf;
};

#include "Obligations.h"

namespace verutop {

std::string Obligation::what() const {
  switch (kind) {
  case ObKind::Postcondition: return "postcondition";
  case ObKind::Precondition: return "precondition of " + subject;
  case ObKind::Assertion: return "assertion";
  case ObKind::InvariantOnEntry: return "loop invariant holds on entry";
  case ObKind::InvariantPreserved: return "loop invariant is preserved";
  case ObKind::LoopMeasure: return "loop measure decreases";
  case ObKind::CallMeasure: return "recursive call decreases the measure";
  case ObKind::MutualCallMeasure: return "mutually recursive call decreases the measure";
  case ObKind::DivisorNotZero: return "divisor is not zero";
  case ObKind::NoOverflow: return subject + " does not overflow";
  case ObKind::ErrorUnreachable: return "error statement is unreachable";
  case ObKind::BorrowsDisjoint: return "borrowed arguments do not overlap";
  case ObKind::MissingLoopMeasure: return "loop needs a decreases clause";
  case ObKind::MissingCallMeasure: return "recursive call needs a decreases clause";
  case ObKind::MissingMutualCallMeasure:
    return "mutually recursive call needs a decreases clause on " + subject;
  case ObKind::LoopMayNotTerminate: return "loop may not terminate; " + subject + " needs decreases *";
  case ObKind::CallMayNotTerminate:
    return callee + " may not terminate; " + subject + " needs decreases *";
  }
  return "";
}

bool Obligation::missing() const {
  switch (kind) {
  case ObKind::MissingLoopMeasure:
  case ObKind::MissingCallMeasure:
  case ObKind::MissingMutualCallMeasure:
  case ObKind::LoopMayNotTerminate:
  case ObKind::CallMayNotTerminate: return true;
  default: return false;
  }
}

bool Obligation::unreachable() const {
  return missing() || kind == ObKind::ErrorUnreachable || kind == ObKind::BorrowsDisjoint;
}

} // namespace verutop

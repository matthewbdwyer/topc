#include "Substituter.h"
#include "Copier.h"
#include "TipBorrowRef.h"
#include "TipOwningRef.h"
#include "TipSumType.h"

#include <algorithm>
#include <iterator>

std::shared_ptr<TipType> Substituter::substitute(TipType *t, TipVar *v,
                                                 std::shared_ptr<TipType> s) {
  Substituter visitor(v, s);
  t->accept(&visitor);
  return visitor.getResult();
}

std::shared_ptr<TipType> Substituter::getResult() {
  return visitedTypes.back();
}

void Substituter::endVisit(TipFunction *element) {
  std::vector<std::shared_ptr<TipType>> argTypes;
  for (auto &arg : element->getArguments()) {
    argTypes.push_back(std::move(visitedTypes.back()));
    visitedTypes.pop_back();
  }

  // the post-order visit will reverse the arguments in visitedTypes
  // so we set them right here
  std::reverse(argTypes.begin(), argTypes.end());

  std::shared_ptr<TipType> retType = argTypes.back();
  argTypes.pop_back();
  visitedTypes.push_back(std::make_shared<TipFunction>(argTypes, retType));
}

void Substituter::endVisit(TipInt *element) {
  // Zero element in visitedTypes (a special case of Cons)
  visitedTypes.push_back(std::make_shared<TipInt>());
}

void Substituter::endVisit(TipMu *element) {
  // Two elements in visitedTypes
  auto tType = visitedTypes.back();
  visitedTypes.pop_back();

  // The second element on the LIFO is always a TipVar
  auto vType = std::dynamic_pointer_cast<TipVar>(visitedTypes.back());
  visitedTypes.pop_back();

  visitedTypes.push_back(std::make_shared<TipMu>(vType, tType));
}

void Substituter::endVisit(TipRecord *element) {
  std::vector<std::shared_ptr<TipType>> initTypes;
  for (auto &init : element->getArguments()) {
    initTypes.push_back(std::move(visitedTypes.back()));
    visitedTypes.pop_back();
  }

  // the post-order visit will reverse the arguments in visitedTypes
  // so we set them right here
  std::reverse(initTypes.begin(), initTypes.end());

  visitedTypes.push_back(
      std::make_shared<TipRecord>(initTypes, element->getNames()));
}

void Substituter::endVisit(TipAbsentField *element) {
  // Zero element in visitedTypes (a special case of Cons)
  visitedTypes.push_back(std::make_shared<TipAbsentField>());
}

void Substituter::endVisit(TipRef *element) {
  // One element in visitedTypes (a special case of Cons)
  auto pointedToType = visitedTypes.back();
  visitedTypes.pop_back();
  visitedTypes.push_back(std::make_shared<TipRef>(pointedToType));
}

void Substituter::endVisit(TipOwningRef *element) {
  auto pointedToType = visitedTypes.back();
  visitedTypes.pop_back();
  visitedTypes.push_back(std::make_shared<TipOwningRef>(pointedToType));
}

void Substituter::endVisit(TipBorrowRef *element) {
  auto pointedToType = visitedTypes.back();
  visitedTypes.pop_back();
  visitedTypes.push_back(std::make_shared<TipBorrowRef>(pointedToType));
}

void Substituter::endVisit(TipSumType *element) {
  std::vector<std::shared_ptr<TipType>> payloads;
  for (std::size_t i = 0; i < element->getArguments().size(); i++) {
    payloads.push_back(visitedTypes.back());
    visitedTypes.pop_back();
  }
  std::reverse(payloads.begin(), payloads.end());
  visitedTypes.push_back(std::make_shared<TipSumType>(
      element->getTypeName(), element->getCtorOrder(), payloads,
      element->getCtorArities()));
}

/*! \brief Substitute if variable is the target.
 */
void Substituter::endVisit(TipVar *element) {
  if (*element == *target) {
    auto copy = Copier::copy(substitution);
    visitedTypes.push_back(copy);
  } else {
    visitedTypes.push_back(std::make_shared<TipVar>(element->getNode()));
  }
}

void Substituter::endVisit(TipAlpha *element) {
  if (*element == *target) {
    auto copy = Copier::copy(substitution);
    visitedTypes.push_back(copy);
  } else {
    visitedTypes.push_back(
        std::make_shared<TipAlpha>(element->getNode(), element->getContext(), element->getName()));
  }
}

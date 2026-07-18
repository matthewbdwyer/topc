#include "Substituter.h"
#include "Copier.h"
#include "TopBorrowRef.h"
#include "TopOwningRef.h"
#include "TopSumType.h"

#include <algorithm>
#include <iterator>

std::shared_ptr<TopType> Substituter::substitute(TopType *t, TopVar *v,
                                                 std::shared_ptr<TopType> s) {
  Substituter visitor(v, s);
  t->accept(&visitor);
  return visitor.getResult();
}

std::shared_ptr<TopType> Substituter::getResult() {
  return visitedTypes.back();
}

void Substituter::endVisit(TopFunction *element) {
  std::vector<std::shared_ptr<TopType>> argTypes;
  for (auto &arg : element->getArguments()) {
    argTypes.push_back(std::move(visitedTypes.back()));
    visitedTypes.pop_back();
  }

  // the post-order visit will reverse the arguments in visitedTypes
  // so we set them right here
  std::reverse(argTypes.begin(), argTypes.end());

  std::shared_ptr<TopType> retType = argTypes.back();
  argTypes.pop_back();
  visitedTypes.push_back(std::make_shared<TopFunction>(argTypes, retType));
}

void Substituter::endVisit(TopInt *element) {
  // Zero element in visitedTypes (a special case of Cons)
  visitedTypes.push_back(std::make_shared<TopInt>());
}

void Substituter::endVisit(TopAbsentField *element) {
  visitedTypes.push_back(std::make_shared<TopAbsentField>());
}

void Substituter::endVisit(TopMu *element) {
  // Two elements in visitedTypes
  auto tType = visitedTypes.back();
  visitedTypes.pop_back();

  // The second element on the LIFO is always a TopVar
  auto vType = std::dynamic_pointer_cast<TopVar>(visitedTypes.back());
  visitedTypes.pop_back();

  visitedTypes.push_back(std::make_shared<TopMu>(vType, tType));
}

void Substituter::endVisit(TopRef *element) {
  // One element in visitedTypes (a special case of Cons)
  auto pointedToType = visitedTypes.back();
  visitedTypes.pop_back();
  visitedTypes.push_back(std::make_shared<TopRef>(pointedToType));
}

void Substituter::endVisit(TopOwningRef *element) {
  auto pointedToType = visitedTypes.back();
  visitedTypes.pop_back();
  visitedTypes.push_back(std::make_shared<TopOwningRef>(pointedToType));
}

void Substituter::endVisit(TopBorrowRef *element) {
  auto pointedToType = visitedTypes.back();
  visitedTypes.pop_back();
  visitedTypes.push_back(std::make_shared<TopBorrowRef>(pointedToType));
}

void Substituter::endVisit(TopRecord *element) {
  std::vector<std::shared_ptr<TopType>> fieldTypes;
  for (std::size_t i = 0; i < element->getInits().size(); i++) {
    fieldTypes.push_back(visitedTypes.back());
    visitedTypes.pop_back();
  }
  std::reverse(fieldTypes.begin(), fieldTypes.end());
  visitedTypes.push_back(
      std::make_shared<TopRecord>(fieldTypes, element->getNames()));
}

void Substituter::endVisit(TopSumType *element) {
  std::vector<std::shared_ptr<TopType>> payloads;
  for (std::size_t i = 0; i < element->getArguments().size(); i++) {
    payloads.push_back(visitedTypes.back());
    visitedTypes.pop_back();
  }
  std::reverse(payloads.begin(), payloads.end());
  visitedTypes.push_back(std::make_shared<TopSumType>(
      element->getTypeName(), element->getCtorOrder(), payloads,
      element->getCtorArities()));
}

/*! \brief Substitute if variable is the target.
 */
void Substituter::endVisit(TopVar *element) {
  if (*element == *target) {
    auto copy = Copier::copy(substitution);
    visitedTypes.push_back(copy);
  } else {
    visitedTypes.push_back(std::make_shared<TopVar>(element->getNode()));
  }
}

void Substituter::endVisit(TopAlpha *element) {
  if (*element == *target) {
    auto copy = Copier::copy(substitution);
    visitedTypes.push_back(copy);
  } else {
    visitedTypes.push_back(
        std::make_shared<TopAlpha>(element->getNode(), element->getContext(), element->getName()));
  }
}

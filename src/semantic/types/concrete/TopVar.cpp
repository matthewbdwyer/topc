#include "TopVar.h"
#include "TopAlpha.h"
#include "TopTypeVisitor.h"
#include "ASTNode.h"

#include <iostream>
#include <sstream>

TopVar::TopVar(ASTNode *node) : node(node){};

ASTNode *TopVar::getNode() const { return node; }

bool TopVar::operator==(const TopType &other) const {
  auto otherTopVar = dynamic_cast<TopVar const *>(&other);
  auto otherTopAlpha = dynamic_cast<TopAlpha const *>(&other);
  if (!otherTopVar || otherTopAlpha) {
    return false;
  }

  return node == otherTopVar->getNode();
}

bool TopVar::operator!=(const TopType &other) const {
  return !(*this == other);
}

bool TopVar::operator<(const TopVar &other) const {
  // Distinguish TopVar from TopAlpha so that TopVarValueCmp never treats
  // a plain TopVar and a TopAlpha as the same element in a set.
  bool thisIsAlpha  = (dynamic_cast<const TopAlpha *>(this)  != nullptr);
  bool otherIsAlpha = (dynamic_cast<const TopAlpha *>(&other) != nullptr);
  if (thisIsAlpha != otherIsAlpha)
    return otherIsAlpha; // TopVar < TopAlpha (arbitrary but consistent)
  // Both same kind: primary key is node address
  if (node != other.node)
    return node < other.node;
  // Both TopVar with same node: equal
  if (!thisIsAlpha)
    return false;
  // Both TopAlpha with same node: secondary key is name then context
  auto thisA  = static_cast<const TopAlpha *>(this);
  auto otherA = static_cast<const TopAlpha *>(&other);
  if (thisA->getName() != otherA->getName())
    return thisA->getName() < otherA->getName();
  return thisA->getContext() < otherA->getContext();
}

std::ostream &TopVar::print(std::ostream &out) const {
  out << "\u27E6" << *node << "@" << node->getLine() << ":" << node->getColumn()
      << "\u27E7";
  return out;
}

void TopVar::accept(TopTypeVisitor *visitor) {
  visitor->visit(this);
  visitor->endVisit(this);
}

std::string TopVar::getFunctor() const {
  // Use the node's address as a unique identifier for the variable
  std::ostringstream oss;
  oss << "var@" << node;
  return oss.str();
}

std::shared_ptr<TopType> TopVar::withChildTypes(
    std::vector<std::shared_ptr<TopType>> children) const {
  if (!children.empty()) {
    throw std::invalid_argument("TopVar has no child types");
  }
  return std::make_shared<TopVar>(node);
}

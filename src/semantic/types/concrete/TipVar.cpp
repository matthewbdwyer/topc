#include "TipVar.h"
#include "TipAlpha.h"
#include "TipTypeVisitor.h"
#include "ASTNode.h"

#include <iostream>
#include <sstream>

TipVar::TipVar(ASTNode *node) : node(node){};

ASTNode *TipVar::getNode() const { return node; }

bool TipVar::operator==(const TipType &other) const {
  auto otherTipVar = dynamic_cast<TipVar const *>(&other);
  auto otherTipAlpha = dynamic_cast<TipAlpha const *>(&other);
  if (!otherTipVar || otherTipAlpha) {
    return false;
  }

  return node == otherTipVar->getNode();
}

bool TipVar::operator!=(const TipType &other) const {
  return !(*this == other);
}

std::ostream &TipVar::print(std::ostream &out) const {
  out << "\u27E6" << *node << "@" << node->getLine() << ":" << node->getColumn()
      << "\u27E7";
  return out;
}

void TipVar::accept(TipTypeVisitor *visitor) {
  visitor->visit(this);
  visitor->endVisit(this);
}

std::string TipVar::getFunctor() const {
  // Use the node's address as a unique identifier for the variable
  std::ostringstream oss;
  oss << "var@" << node;
  return oss.str();
}

std::shared_ptr<Term> TipVar::withSubterms(std::vector<std::shared_ptr<Term>> newSubterms) const {
  if (!newSubterms.empty()) {
    throw std::invalid_argument("TipVar has no subterms");
  }
  return std::make_shared<TipVar>(node);
}

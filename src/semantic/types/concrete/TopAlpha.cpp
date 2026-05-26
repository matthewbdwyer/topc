#include "TopAlpha.h"
#include "TopTypeVisitor.h"
#include "ASTNode.h"
#include "loguru.hpp"
#include <sstream>

TopAlpha::TopAlpha(ASTNode *node) : TopVar(node), context(nullptr), name(""){};

TopAlpha::TopAlpha(ASTNode *node, std::string const name)
    : TopVar(node), context(nullptr), name(name){};

TopAlpha::TopAlpha(ASTNode *node, ASTNode *context, std::string const name)
    : TopVar(node), context(context), name(name){};

std::ostream &TopAlpha::print(std::ostream &out) const {
  out << "\u03B1<" << *node << "@" << node->getLine() << ":"
      << node->getColumn();
  if (context != nullptr) {
    out << "{" << *context << "@" << context->getLine() << ":"
        << context->getColumn() << "}";
  }
  if (name == "") {
    out << ">";
  } else {
    out << "[" << name << "]>";
  }
  return out;
}

bool TopAlpha::operator==(const TopType &other) const {
  auto otherTopAlpha = dynamic_cast<const TopAlpha *>(&other);
  if (!otherTopAlpha) {
    return false;
  }

  return node == otherTopAlpha->getNode() &&
         context == otherTopAlpha->getContext() &&
         name == otherTopAlpha->getName();
}

bool TopAlpha::operator!=(const TopType &other) const {
  return !(*this == other);
}

ASTNode *TopAlpha::getContext() const { return context; }

std::string const &TopAlpha::getName() const { return name; }

void TopAlpha::accept(TopTypeVisitor *visitor) { visitor->endVisit(this); }

std::string TopAlpha::getFunctor() const {
  return "α" + name;
}

std::shared_ptr<TopType> TopAlpha::withChildTypes(
    std::vector<std::shared_ptr<TopType>> children) const {
  if (!children.empty()) {
    throw std::invalid_argument("TopAlpha has no child types");
  }
  return std::make_shared<TopAlpha>(node, context, name);
}

#include "ASTVisualizer.h"
#include "AST.h"
#include "Iterator.h"

#include <iostream>
#include <sstream>

namespace {
std::string render(const ASTNode &node) {
  std::ostringstream text;
  text << node;
  return text.str();
}

std::string nodeLabel(ASTNode *node) {
  if (dynamic_cast<ASTProgram *>(node))
    return "Program";
  if (auto *function = dynamic_cast<ASTFunction *>(node))
    return "Function: " + function->getName();
  if (dynamic_cast<ASTCaseStmt *>(node))
    return "CaseStmt";
  if (dynamic_cast<ASTCaseArm *>(node))
    return "CaseArm";
  if (dynamic_cast<ASTAssignStmt *>(node))
    return "AssignStmt";
  if (auto *binary = dynamic_cast<ASTBinaryExpr *>(node))
    return "BinaryExpr: " + binary->getOp();
  if (auto *variable = dynamic_cast<ASTVariableExpr *>(node))
    return "VariableExpr: " + variable->getName();
  if (auto *number = dynamic_cast<ASTNumberExpr *>(node))
    return "IntegerLiteral: " + std::to_string(number->getValue());
  if (dynamic_cast<ASTReturnStmt *>(node))
    return "ReturnStmt";
  if (dynamic_cast<ASTFunAppExpr *>(node))
    return "CallExpr";
  if (dynamic_cast<ASTIfStmt *>(node))
    return "IfStmt";
  if (dynamic_cast<ASTWhileStmt *>(node))
    return "WhileStmt";
  if (dynamic_cast<ASTBlockStmt *>(node))
    return "BlockStmt";
  if (dynamic_cast<ASTDeclStmt *>(node))
    return "DeclarationStmt: " + render(*node);
  if (dynamic_cast<ASTDeclNode *>(node))
    return "Declaration: " + render(*node);
  if (dynamic_cast<ASTSumTypeDecl *>(node))
    return "TypeDeclaration: " + render(*node);
  if (dynamic_cast<ASTSumVariant *>(node))
    return "ConstructorDeclaration: " + render(*node);
  if (dynamic_cast<ASTSumCtorExpr *>(node))
    return "ConstructorExpr: " + render(*node);
  if (dynamic_cast<ASTAllocExpr *>(node))
    return "AllocExpr";
  if (dynamic_cast<ASTBorrowExpr *>(node))
    return "BorrowExpr";
  if (dynamic_cast<ASTDeRefExpr *>(node))
    return "DerefExpr";
  if (dynamic_cast<ASTInputExpr *>(node))
    return "InputExpr";
  if (dynamic_cast<ASTOutputStmt *>(node))
    return "OutputStmt";
  if (dynamic_cast<ASTErrorStmt *>(node))
    return "ErrorStmt";
  if (dynamic_cast<ASTDestroyStmt *>(node))
    return "DestroyStmt";
  return "ASTNode: " + render(*node);
}

std::string patternLabel(ASTPattern *pattern) {
  if (auto *constructor = dynamic_cast<ASTCtorPattern *>(pattern))
    return "ConstructorPattern: " + constructor->getTag();
  if (auto *variable = dynamic_cast<ASTVarPattern *>(pattern))
    return "BindingPattern: " + variable->getName();
  if (dynamic_cast<ASTWildcardPattern *>(pattern))
    return "WildcardPattern: _";
  return "Pattern";
}

std::string childPrefix(const std::string &prefix, bool isLast) {
  return prefix + (isLast ? "    " : "│   ");
}

std::string indexedRole(const std::string &role, std::size_t index) {
  return role + "[" + std::to_string(index) + "]";
}

std::string childRole(ASTNode *parent, std::size_t index) {
  if (auto *program = dynamic_cast<ASTProgram *>(parent)) {
    const auto typeCount = program->getTypedecls().size();
    return index < typeCount ? indexedRole("type", index)
                             : indexedRole("function", index - typeCount);
  }
  if (auto *function = dynamic_cast<ASTFunction *>(parent)) {
    const auto formalCount = function->getFormals().size();
    const auto declarationCount = function->getDeclarations().size();
    if (index == 0)
      return "name";
    if (index <= formalCount)
      return indexedRole("parameter", index - 1);
    if (index <= formalCount + declarationCount)
      return indexedRole("declaration", index - formalCount - 1);
    return indexedRole("statement",
                       index - formalCount - declarationCount - 1);
  }
  if (dynamic_cast<ASTReturnStmt *>(parent))
    return "value";
  if (dynamic_cast<ASTOutputStmt *>(parent) ||
      dynamic_cast<ASTErrorStmt *>(parent))
    return "value";
  if (dynamic_cast<ASTAllocExpr *>(parent))
    return "initializer";
  if (dynamic_cast<ASTBorrowExpr *>(parent))
    return "borrowed";
  if (dynamic_cast<ASTDeRefExpr *>(parent))
    return "reference";
  if (dynamic_cast<ASTWhileStmt *>(parent))
    return index == 0 ? "condition" : "body";
  if (dynamic_cast<ASTIfStmt *>(parent)) {
    if (index == 0)
      return "condition";
    return index == 1 ? "then" : "else";
  }
  if (dynamic_cast<ASTFunAppExpr *>(parent))
    return index == 0 ? "callee" : indexedRole("argument", index - 1);
  if (dynamic_cast<ASTSumCtorExpr *>(parent))
    return indexedRole("payload", index);
  if (dynamic_cast<ASTSumTypeDecl *>(parent))
    return indexedRole("variant", index);
  if (dynamic_cast<ASTSumVariant *>(parent))
    return indexedRole("payload", index);
  if (dynamic_cast<ASTDeclStmt *>(parent))
    return indexedRole("binding", index);
  if (dynamic_cast<ASTBlockStmt *>(parent))
    return indexedRole("statement", index);
  return indexedRole("child", index);
}
} // namespace

void ASTVisualizer::emitAsciiPattern(ASTPattern *pattern,
                                     const std::string &prefix, bool isLast,
                                     const std::string &role) {
  os << prefix << (isLast ? "└── " : "├── ") << role << ": "
     << patternLabel(pattern) << "\n";

  auto *constructor = dynamic_cast<ASTCtorPattern *>(pattern);
  if (!constructor)
    return;

  auto subpatterns = constructor->getSubPatterns();
  const auto nextPrefix = childPrefix(prefix, isLast);
  for (std::size_t i = 0; i < subpatterns.size(); ++i) {
    emitAsciiPattern(subpatterns[i], nextPrefix, i + 1 == subpatterns.size(),
                     "payload[" + std::to_string(i) + "]");
  }
}

void ASTVisualizer::emitAsciiNode(ASTNode *node, const std::string &prefix,
                                  bool isLast, const std::string &role) {
  os << prefix << (isLast ? "└── " : "├── ");
  if (!role.empty())
    os << role << ": ";
  os << nodeLabel(node) << "\n";

  const auto nextPrefix = childPrefix(prefix, isLast);

  if (auto *caseStmt = dynamic_cast<ASTCaseStmt *>(node)) {
    auto arms = caseStmt->getArms();
    emitAsciiNode(caseStmt->getCaseExpr(), nextPrefix, arms.empty(),
            "case-expression");
    for (std::size_t i = 0; i < arms.size(); ++i) {
      emitAsciiNode(arms[i], nextPrefix, i + 1 == arms.size(),
                    "arm[" + std::to_string(i) + "]");
    }
    return;
  }

  if (auto *arm = dynamic_cast<ASTCaseArm *>(node)) {
    auto patterns = arm->getPatterns();
    const bool hasBody = arm->getBody() != nullptr;
    if (patterns.empty()) {
      os << nextPrefix << (hasBody ? "├── " : "└── ")
         << "pattern: ConstructorPattern: " << arm->getTag() << "\n";
    } else {
      os << nextPrefix << "├── pattern: ConstructorPattern: " << arm->getTag()
         << "\n";
      const std::string patternPrefix = nextPrefix + "│   ";
      for (std::size_t i = 0; i < patterns.size(); ++i) {
        emitAsciiPattern(patterns[i], patternPrefix,
                         i + 1 == patterns.size(),
                         "payload[" + std::to_string(i) + "]");
      }
    }
    if (hasBody)
      emitAsciiNode(arm->getBody(), nextPrefix, true, "body");
    return;
  }

  if (auto *assignment = dynamic_cast<ASTAssignStmt *>(node)) {
    emitAsciiNode(assignment->getLHS(), nextPrefix, false, "lhs");
    emitAsciiNode(assignment->getRHS(), nextPrefix, true, "rhs");
    return;
  }

  if (auto *binary = dynamic_cast<ASTBinaryExpr *>(node)) {
    emitAsciiNode(binary->getLeft(), nextPrefix, false, "lhs");
    emitAsciiNode(binary->getRight(), nextPrefix, true, "rhs");
    return;
  }

  auto children = node->getChildren();
  for (std::size_t i = 0; i < children.size(); ++i) {
    emitAsciiNode(children[i].get(), nextPrefix, i + 1 == children.size(),
                  childRole(node, i));
  }
}

void ASTVisualizer::buildAscii(SyntaxTree &tree) {
  os << nodeLabel(tree.getRoot().get()) << "\n";
  auto children = tree.getRoot()->getChildren();
  for (std::size_t i = 0; i < children.size(); ++i) {
    emitAsciiNode(children[i].get(), "", i + 1 == children.size(),
                  childRole(tree.getRoot().get(), i));
  }
}

void ASTVisualizer::buildGraph(SyntaxTree &tree) {
  os << "digraph {"
     << "\n";
  os << "  rankdir = TB;"
     << "\n";
  os << "\n";

  for (auto iter = tree.begin(""); iter != tree.end(""); ++iter) {
    auto node = iter->getRoot().get();
    if (parent.empty()) {
      std::string vertexName = "v" + std::to_string(vertexMap.size());
      vertexMap.insert(std::pair<ASTNode *, std::string>(node, vertexName));
      declare_node(node, "start");
      pushn(node, node->getChildren().size());
    } else {
      process_node(node);
    }
  }

  os << "}";
}

void ASTVisualizer::process_node(ASTNode *element) {
  /*
   * We construct names for vertices in the dot representation to make
   * the visualizer output consistent across runs on the same program.
   * The assumption is that nodes are processed in pre-order so that
   * the name of the parent is generated before a child is processed.
   */
  std::string vertexName = "v" + std::to_string(vertexMap.size());
  vertexMap.insert(std::pair<ASTNode *, std::string>(element, vertexName));

  declare_node(element);
  connect_node_to_parent(element);
  pushn(element, element->getChildren().size());
}

void ASTVisualizer::declare_node(ASTNode *element, std::string label) {
  std::stringstream l(label);
  if (label.empty()) {
    l << *element;
  }
  os << "  " << '"' << vertexMap[element] << '"' << " [label = " << '"'
     << l.str() << '"' << "];"
     << "\n";
}

void ASTVisualizer::connect_node_to_parent(ASTNode *element) {
  if (parent.empty()) {
    return;
  }

  os << "  " << '"' << vertexMap[parent.top()] << '"' << " -> " << '"'
     << vertexMap[element] << '"' << ";\n";
  parent.pop();
}

void ASTVisualizer::pushn(ASTNode *element, int n) {
  for (int i = 0; i < n; i++) {
    parent.push(element);
  }
}

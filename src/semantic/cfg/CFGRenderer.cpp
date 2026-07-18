#include "CFGRenderer.h"

#include "ASTFunction.h"
#include "ControlFlowGraph.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

namespace {
std::string escapeDot(std::string text) {
  std::string escaped;
  escaped.reserve(text.size());
  for (char c : text) {
    switch (c) {
    case '\\':
      escaped += "\\\\";
      break;
    case '"':
      escaped += "\\\"";
      break;
    case '\n':
      escaped += "\\n";
      break;
    default:
      escaped += c;
      break;
    }
  }
  return escaped;
}

std::string blockText(const BasicBlock &block) {
  if (block.getName() == "entry" || block.getName() == "exit") {
    return block.getName();
  }

  if (block.getTerminatorStatement() != nullptr) {
    std::ostringstream text;
    text << *block.getTerminatorStatement();
    return text.str();
  }

  if (!block.getStatements().empty()) {
    std::ostringstream text;
    for (std::size_t i = 0; i < block.getStatements().size(); ++i) {
      if (i > 0) {
        text << " | ";
      }
      text << *block.getStatements()[i];
    }
    return text.str();
  }

  return block.getName();
}

int edgeOrder(const CFGEdge &edge) {
  switch (edge.kind) {
  case CFGEdgeKind::TrueBranch:
    return 0;
  case CFGEdgeKind::FalseBranch:
    return 1;
  case CFGEdgeKind::CaseArm:
    return 2;
  case CFGEdgeKind::Fallthrough:
    return 3;
  case CFGEdgeKind::ReturnToExit:
    return 4;
  }
  return 99;
}

std::string edgeLabel(const CFGEdge &edge) {
  switch (edge.kind) {
  case CFGEdgeKind::TrueBranch:
    return "true";
  case CFGEdgeKind::FalseBranch:
    return "false";
  case CFGEdgeKind::CaseArm:
    return edge.label;
  case CFGEdgeKind::Fallthrough:
    return "fallthrough";
  case CFGEdgeKind::ReturnToExit:
    return "return";
  }
  return "";
}
} // namespace

void CFGRenderer::renderAscii(const ControlFlowGraph &cfg, std::ostream &os) {
  os << "[cfg " << cfg.getFunction()->getName() << "]\n";

  for (const auto &block : cfg.getBlocks()) {
    if (block.getName() == "entry") {
      if (!block.getSuccessors().empty()) {
        auto *target = cfg.findBlock(block.getSuccessors()[0].target);
        os << "  entry -> " << (target ? target->getName() : "<missing>")
           << "\n";
      } else {
        os << "  entry\n";
      }
      continue;
    }

    if (block.getName() == "exit") {
      os << "  exit\n";
      continue;
    }

    os << "  " << block.getName() << ": " << blockText(block) << "\n";

    auto successors = block.getSuccessors();
    std::stable_sort(successors.begin(), successors.end(),
                     [](const auto &a, const auto &b) {
                       return edgeOrder(a) < edgeOrder(b);
                     });

    for (const auto &edge : successors) {
      auto *target = cfg.findBlock(edge.target);
      os << "    " << edgeLabel(edge) << " -> "
         << (target ? target->getName() : "<missing>") << "\n";
    }
  }
}

void CFGRenderer::renderDot(const ControlFlowGraph &cfg, std::ostream &os) {
  os << "digraph \"" << cfg.getFunction()->getName() << "\" {\n";

  for (const auto &block : cfg.getBlocks()) {
    os << "  " << block.getName() << " [label=\""
       << escapeDot(blockText(block)) << "\"];\n";
  }

  for (const auto &block : cfg.getBlocks()) {
    auto successors = block.getSuccessors();
    std::stable_sort(successors.begin(), successors.end(),
                     [](const auto &a, const auto &b) {
                       return edgeOrder(a) < edgeOrder(b);
                     });
    for (const auto &edge : successors) {
      auto *target = cfg.findBlock(edge.target);
      if (target == nullptr) {
        continue;
      }
      os << "  " << block.getName() << " -> " << target->getName();
      const auto label = edgeLabel(edge);
      if (!label.empty() && edge.kind != CFGEdgeKind::Fallthrough) {
        os << " [label=\"" << escapeDot(label) << "\"]";
      }
      os << ";\n";
    }
  }

  os << "}\n";
}

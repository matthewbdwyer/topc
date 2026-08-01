#include "ControlFlowGraph.h"

#include "InternalError.h"

#include <queue>
#include <set>
#include <sstream>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

namespace {
[[noreturn]] void fail(const std::string &msg) { throw InternalError(msg); }
}

ControlFlowGraph::ControlFlowGraph(const ASTFunction *function,
                                   std::vector<BasicBlock> blocks,
                                   BlockId entryId, BlockId exitId)
    : function(function), blocks(std::move(blocks)), entryId(entryId),
      exitId(exitId) {}

const ASTFunction *ControlFlowGraph::getFunction() const { return function; }

const BasicBlock &ControlFlowGraph::getEntry() const {
  auto *entry = findBlock(entryId);
  if (entry == nullptr) {
    fail("CFG entry block does not exist");
  }
  return *entry;
}

const BasicBlock &ControlFlowGraph::getExit() const {
  auto *exit = findBlock(exitId);
  if (exit == nullptr) {
    fail("CFG exit block does not exist");
  }
  return *exit;
}

const std::vector<BasicBlock> &ControlFlowGraph::getBlocks() const {
  return blocks;
}

const BasicBlock *ControlFlowGraph::findBlock(BlockId id) const {
  for (const auto &block : blocks) {
    if (block.getId() == id) {
      return &block;
    }
  }
  return nullptr;
}

void ControlFlowGraph::validate() const {
  if (blocks.empty()) {
    fail("CFG must contain at least one block");
  }

  std::unordered_map<BlockId, const BasicBlock *> byId;
  std::unordered_set<std::string> names;

  for (const auto &block : blocks) {
    if (byId.find(block.getId()) != byId.end()) {
      fail("CFG contains duplicate block id");
    }
    byId[block.getId()] = &block;

    if (!names.insert(block.getName()).second) {
      fail("CFG contains duplicate block name");
    }
  }

  auto entryIt = byId.find(entryId);
  auto exitIt = byId.find(exitId);
  if (entryIt == byId.end()) {
    fail("CFG entry block id is missing");
  }
  if (exitIt == byId.end()) {
    fail("CFG exit block id is missing");
  }

  const auto *entry = entryIt->second;
  const auto *exit = exitIt->second;

  if (!entry->getPredecessors().empty()) {
    fail("CFG entry block must not have predecessors");
  }
  if (!exit->getSuccessors().empty()) {
    fail("CFG exit block must not have successors");
  }

  for (const auto &block : blocks) {
    std::set<std::tuple<BlockId, CFGEdgeKind, std::string>> uniqueEdges;
    for (const auto &edge : block.getSuccessors()) {
      if (byId.find(edge.target) == byId.end()) {
        fail("CFG contains edge to unknown target block");
      }

      if (!uniqueEdges
               .insert(std::make_tuple(edge.target, edge.kind, edge.label))
               .second) {
        fail("CFG block contains duplicate successor edge");
      }

      const auto *target = byId.at(edge.target);
      bool hasReversePred = false;
      for (auto pred : target->getPredecessors()) {
        if (pred == block.getId()) {
          hasReversePred = true;
          break;
        }
      }
      if (!hasReversePred) {
        fail("CFG successor/predecessor relation is inconsistent");
      }
    }

    for (auto pred : block.getPredecessors()) {
      auto predIt = byId.find(pred);
      if (predIt == byId.end()) {
        fail("CFG block has unknown predecessor");
      }
      const auto *predBlock = predIt->second;
      bool hasForwardEdge = false;
      for (const auto &edge : predBlock->getSuccessors()) {
        if (edge.target == block.getId()) {
          hasForwardEdge = true;
          break;
        }
      }
      if (!hasForwardEdge) {
        fail("CFG predecessor/successor relation is inconsistent");
      }
    }

    switch (block.getTerminatorKind()) {
    case CFGTerminatorKind::If:
    case CFGTerminatorKind::While: {
      int trueCount = 0;
      int falseCount = 0;
      for (const auto &edge : block.getSuccessors()) {
        if (edge.kind == CFGEdgeKind::TrueBranch) {
          trueCount++;
        }
        if (edge.kind == CFGEdgeKind::FalseBranch) {
          falseCount++;
        }
      }
      if (trueCount != 1 || falseCount != 1) {
        fail("CFG conditional block must have exactly one true and one false successor");
      }
      break;
    }
    case CFGTerminatorKind::Return:
      if (block.getSuccessors().size() != 1 ||
          block.getSuccessors()[0].kind != CFGEdgeKind::ReturnToExit ||
          block.getSuccessors()[0].target != exitId) {
        fail("CFG return block must have exactly one ReturnToExit edge to exit");
      }
      break;
    case CFGTerminatorKind::Case:
      if (block.getSuccessors().empty()) {
        fail("CFG case block must have at least one arm successor");
      }
      for (const auto &edge : block.getSuccessors()) {
        if (edge.kind != CFGEdgeKind::CaseArm) {
          fail("CFG case block successors must be CaseArm edges");
        }
      }
      break;
    case CFGTerminatorKind::Fallthrough:
      break;
    }
  }

  std::unordered_set<BlockId> visited;
  std::queue<BlockId> work;
  work.push(entryId);
  visited.insert(entryId);

  while (!work.empty()) {
    auto id = work.front();
    work.pop();
    for (const auto &edge : byId.at(id)->getSuccessors()) {
      if (visited.insert(edge.target).second) {
        work.push(edge.target);
      }
    }
  }

  if (visited.size() != blocks.size()) {
    fail("CFG contains unreachable blocks");
  }
}

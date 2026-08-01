#include "cfg/ControlFlowGraph.h"

#include "InternalError.h"

#include <catch2/catch_test_macros.hpp>

#include <vector>

namespace {
BasicBlock makeEntry(std::vector<CFGEdge> succ = {{1, CFGEdgeKind::Fallthrough, ""}},
                     std::vector<BlockId> pred = {}) {
  return BasicBlock(0, "entry", {}, CFGTerminatorKind::Fallthrough, nullptr,
                    nullptr, std::move(succ), std::move(pred));
}

BasicBlock makeMiddle(std::vector<CFGEdge> succ = {{2, CFGEdgeKind::ReturnToExit, "return"}},
                      std::vector<BlockId> pred = {0},
                      CFGTerminatorKind term = CFGTerminatorKind::Return) {
  return BasicBlock(1, "b1", {}, term, nullptr, nullptr, std::move(succ),
                    std::move(pred));
}

BasicBlock makeExit(std::vector<CFGEdge> succ = {}, std::vector<BlockId> pred = {1}) {
  return BasicBlock(2, "exit", {}, CFGTerminatorKind::Fallthrough, nullptr,
                    nullptr, std::move(succ), std::move(pred));
}

ControlFlowGraph makeValidGraph() {
  std::vector<BasicBlock> blocks;
  blocks.push_back(makeEntry());
  blocks.push_back(makeMiddle());
  blocks.push_back(makeExit());
  return ControlFlowGraph(nullptr, std::move(blocks), 0, 2);
}
} // namespace

TEST_CASE("ControlFlowGraph: finds blocks by stable ID", "[cfg][ControlFlowGraph]") {
  auto graph = makeValidGraph();

  REQUIRE_NOTHROW(graph.validate());
  REQUIRE(graph.findBlock(0) != nullptr);
  REQUIRE(graph.findBlock(1) != nullptr);
  REQUIRE(graph.findBlock(2) != nullptr);
  REQUIRE(graph.findBlock(99) == nullptr);
}

TEST_CASE("ControlFlowGraph: rejects dangling successor", "[cfg][ControlFlowGraph]") {
  std::vector<BasicBlock> blocks;
  blocks.push_back(makeEntry({{99, CFGEdgeKind::Fallthrough, ""}}, {}));
  blocks.push_back(makeMiddle());
  blocks.push_back(makeExit());

  ControlFlowGraph graph(nullptr, std::move(blocks), 0, 2);
  REQUIRE_THROWS_AS(graph.validate(), InternalError);
}

TEST_CASE("ControlFlowGraph: rejects inconsistent predecessor", "[cfg][ControlFlowGraph]") {
  std::vector<BasicBlock> blocks;
  blocks.push_back(makeEntry());
  blocks.push_back(makeMiddle({{2, CFGEdgeKind::ReturnToExit, "return"}}, {},
                              CFGTerminatorKind::Return));
  blocks.push_back(makeExit());

  ControlFlowGraph graph(nullptr, std::move(blocks), 0, 2);
  REQUIRE_THROWS_AS(graph.validate(), InternalError);
}

TEST_CASE("ControlFlowGraph: rejects duplicate block ID", "[cfg][ControlFlowGraph]") {
  std::vector<BasicBlock> blocks;
  blocks.push_back(makeEntry());
  blocks.push_back(makeMiddle());
  blocks.push_back(BasicBlock(1, "b1_dup", {}, CFGTerminatorKind::Fallthrough,
                              nullptr, nullptr, {}, {0}));

  ControlFlowGraph graph(nullptr, std::move(blocks), 0, 2);
  REQUIRE_THROWS_AS(graph.validate(), InternalError);
}

TEST_CASE("ControlFlowGraph: rejects entry predecessor", "[cfg][ControlFlowGraph]") {
  std::vector<BasicBlock> blocks;
  blocks.push_back(makeEntry({{1, CFGEdgeKind::Fallthrough, ""}}, {2}));
  blocks.push_back(makeMiddle());
  blocks.push_back(makeExit());

  ControlFlowGraph graph(nullptr, std::move(blocks), 0, 2);
  REQUIRE_THROWS_AS(graph.validate(), InternalError);
}

TEST_CASE("ControlFlowGraph: rejects exit successor", "[cfg][ControlFlowGraph]") {
  std::vector<BasicBlock> blocks;
  blocks.push_back(makeEntry());
  blocks.push_back(makeMiddle());
  blocks.push_back(makeExit({{0, CFGEdgeKind::Fallthrough, ""}}, {1}));

  ControlFlowGraph graph(nullptr, std::move(blocks), 0, 2);
  REQUIRE_THROWS_AS(graph.validate(), InternalError);
}

TEST_CASE("ControlFlowGraph: rejects malformed conditional successors", "[cfg][ControlFlowGraph]") {
  std::vector<BasicBlock> blocks;
  blocks.push_back(makeEntry());
  blocks.push_back(makeMiddle({{2, CFGEdgeKind::TrueBranch, ""}}, {0},
                              CFGTerminatorKind::If));
  blocks.push_back(makeExit());

  ControlFlowGraph graph(nullptr, std::move(blocks), 0, 2);
  REQUIRE_THROWS_AS(graph.validate(), InternalError);
}

TEST_CASE("ControlFlowGraph: rejects malformed return successor", "[cfg][ControlFlowGraph]") {
  std::vector<BasicBlock> blocks;
  blocks.push_back(makeEntry());
  blocks.push_back(makeMiddle({{2, CFGEdgeKind::Fallthrough, ""}}, {0},
                              CFGTerminatorKind::Return));
  blocks.push_back(makeExit());

  ControlFlowGraph graph(nullptr, std::move(blocks), 0, 2);
  REQUIRE_THROWS_AS(graph.validate(), InternalError);
}

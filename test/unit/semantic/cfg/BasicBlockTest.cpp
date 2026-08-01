#include "cfg/BasicBlock.h"

#include <catch2/catch_test_macros.hpp>

#include <vector>

TEST_CASE("BasicBlock: exposes immutable statement sequence", "[cfg][BasicBlock]") {
  const ASTStmt *stmtA = nullptr;
  const ASTStmt *stmtB = nullptr;

  BasicBlock block(
      1,
      "b1",
      std::vector<const ASTStmt *>{stmtA, stmtB},
      CFGTerminatorKind::Fallthrough,
      nullptr,
      nullptr,
      std::vector<CFGEdge>{{2, CFGEdgeKind::Fallthrough, ""}},
      std::vector<BlockId>{0});

  REQUIRE(block.getId() == 1);
  REQUIRE(block.getName() == "b1");
  REQUIRE(block.getStatements().size() == 2);
  REQUIRE(block.getStatements()[0] == stmtA);
  REQUIRE(block.getStatements()[1] == stmtB);
  REQUIRE(block.getPredecessors().size() == 1);
  REQUIRE(block.getPredecessors()[0] == 0);
}

TEST_CASE("BasicBlock: preserves successor labels and kinds", "[cfg][BasicBlock]") {
  BasicBlock block(
      3,
      "b3",
      {},
      CFGTerminatorKind::Case,
      nullptr,
      nullptr,
      std::vector<CFGEdge>{{4, CFGEdgeKind::CaseArm, "Some(x)"},
                           {5, CFGEdgeKind::CaseArm, "None"}},
      std::vector<BlockId>{1, 2});

  REQUIRE(block.getSuccessors().size() == 2);
  REQUIRE(block.getSuccessors()[0].target == 4);
  REQUIRE(block.getSuccessors()[0].kind == CFGEdgeKind::CaseArm);
  REQUIRE(block.getSuccessors()[0].label == "Some(x)");
  REQUIRE(block.getSuccessors()[1].target == 5);
  REQUIRE(block.getSuccessors()[1].kind == CFGEdgeKind::CaseArm);
  REQUIRE(block.getSuccessors()[1].label == "None");
}

#include "cfg/IntraproceduralCFGs.h"

#include "ASTHelper.h"
#include "ASTCaseStmt.h"
#include "ASTIfStmt.h"
#include "ASTWhileStmt.h"
#include "InternalError.h"

#include <catch2/catch_test_macros.hpp>

#include <sstream>

namespace {
std::shared_ptr<ASTProgram> parseProgram(const char *source) {
  std::stringstream program;
  program << source;
  return ASTHelper::build_ast(program);
}

const BasicBlock *findFirstIfBlock(const ControlFlowGraph &cfg) {
  for (const auto &block : cfg.getBlocks()) {
    if (block.getTerminatorKind() == CFGTerminatorKind::If) {
      return &block;
    }
  }
  return nullptr;
}

const BasicBlock *findIfBlockForStmt(const ControlFlowGraph &cfg,
                                     const ASTIfStmt *stmt) {
  for (const auto &block : cfg.getBlocks()) {
    if (block.getTerminatorKind() == CFGTerminatorKind::If &&
        block.getTerminatorStatement() == stmt) {
      return &block;
    }
  }
  return nullptr;
}

const BasicBlock *findWhileBlockForStmt(const ControlFlowGraph &cfg,
                                        const ASTWhileStmt *stmt) {
  for (const auto &block : cfg.getBlocks()) {
    if (block.getTerminatorKind() == CFGTerminatorKind::While &&
        block.getTerminatorStatement() == stmt) {
      return &block;
    }
  }
  return nullptr;
}

ASTWhileStmt *findFirstWhileStmt(ASTFunction *function) {
  for (auto *stmt : function->getStmts()) {
    if (auto *whileStmt = dynamic_cast<ASTWhileStmt *>(stmt)) {
      return whileStmt;
    }
  }
  return nullptr;
}

const BasicBlock *findCaseBlockForStmt(const ControlFlowGraph &cfg,
                                       const ASTCaseStmt *stmt) {
  for (const auto &block : cfg.getBlocks()) {
    if (block.getTerminatorKind() == CFGTerminatorKind::Case &&
        block.getTerminatorStatement() == stmt) {
      return &block;
    }
  }
  return nullptr;
}

ASTCaseStmt *findFirstCaseStmt(ASTFunction *function) {
  for (auto *stmt : function->getStmts()) {
    if (auto *caseStmt = dynamic_cast<ASTCaseStmt *>(stmt)) {
      return caseStmt;
    }
  }
  return nullptr;
}
} // namespace

TEST_CASE("CFGBuilder: one graph per source function in source order",
          "[cfg][CFGBuilder]") {
  auto ast = parseProgram(R"(
    id(x) {
      return x;
    }

    main() {
      var y;
      y = 1;
      return y;
    }
  )");

  auto cfgs = IntraproceduralCFGs::build(ast.get());
  auto graphs = cfgs->getAll();

  REQUIRE(graphs.size() == 2);
  REQUIRE(graphs[0]->getFunction()->getName() == "id");
  REQUIRE(graphs[1]->getFunction()->getName() == "main");
}

TEST_CASE("CFGBuilder: entry reaches straight-line statements then return",
          "[cfg][CFGBuilder]") {
  auto ast = parseProgram(R"(
    main() {
      var x;
      x = 5;
      output x;
      return x;
    }
  )");

  auto cfgs = IntraproceduralCFGs::build(ast.get());
  auto *f = ast->findFunctionByName("main");
  REQUIRE(f != nullptr);

  const auto &cfg = cfgs->get(f);
  REQUIRE_NOTHROW(cfg.validate());

  const auto &entry = cfg.getEntry();
  REQUIRE(entry.getName() == "entry");
  REQUIRE(entry.getSuccessors().size() == 1);

  auto bodyId = entry.getSuccessors()[0].target;
  auto *body = cfg.findBlock(bodyId);
  REQUIRE(body != nullptr);
  REQUIRE(body->getName() == "b0");
  REQUIRE(body->getTerminatorKind() == CFGTerminatorKind::Return);
  REQUIRE(body->getSuccessors().size() == 1);
  REQUIRE(body->getSuccessors()[0].kind == CFGEdgeKind::ReturnToExit);
  REQUIRE(body->getSuccessors()[0].target == cfg.getExit().getId());
}

TEST_CASE("CFGBuilder: preserves statement order inside a basic block",
          "[cfg][CFGBuilder]") {
  auto ast = parseProgram(R"(
    main() {
      var x, y;
      x = 1;
      y = x;
      output y;
      return y;
    }
  )");

  auto cfgs = IntraproceduralCFGs::build(ast.get());
  auto *f = ast->findFunctionByName("main");
  REQUIRE(f != nullptr);

  const auto &cfg = cfgs->get(f);
  auto bodyId = cfg.getEntry().getSuccessors()[0].target;
  auto *body = cfg.findBlock(bodyId);
  REQUIRE(body != nullptr);

  auto stmts = f->getStmts();
  REQUIRE(stmts.size() >= 4);
  REQUIRE(body->getStatements().size() == stmts.size() - 1);

  for (std::size_t i = 0; i + 1 < stmts.size(); i++) {
    REQUIRE(body->getStatements()[i] == stmts[i]);
  }
}

TEST_CASE("CFGBuilder: return reaches exit and not lexical continuation",
          "[cfg][CFGBuilder]") {
  auto ast = parseProgram(R"(
    main() {
      var x;
      x = 10;
      return x;
    }
  )");

  auto cfgs = IntraproceduralCFGs::build(ast.get());
  auto *f = ast->findFunctionByName("main");
  REQUIRE(f != nullptr);

  const auto &cfg = cfgs->get(f);
  auto bodyId = cfg.getEntry().getSuccessors()[0].target;
  auto *body = cfg.findBlock(bodyId);
  REQUIRE(body != nullptr);

  REQUIRE(body->getTerminatorKind() == CFGTerminatorKind::Return);
  REQUIRE(body->getSuccessors().size() == 1);
  REQUIRE(body->getSuccessors()[0].kind == CFGEdgeKind::ReturnToExit);
  REQUIRE(body->getSuccessors()[0].target == cfg.getExit().getId());
}

TEST_CASE("IntraproceduralCFGs: unknown function lookup is rejected",
          "[cfg][CFGBuilder]") {
  auto ast = parseProgram(R"(
    main() {
      return 0;
    }
  )");

  auto cfgs = IntraproceduralCFGs::build(ast.get());

  auto other = parseProgram(R"(
    helper() {
      return 1;
    }
  )");
  auto *unknown = other->findFunctionByName("helper");
  REQUIRE(unknown != nullptr);

  REQUIRE_THROWS_AS(cfgs->get(unknown), InternalError);
}

TEST_CASE("CFGBuilder: validates every completed graph", "[cfg][CFGBuilder]") {
  auto ast = parseProgram(R"(
    inc(n) {
      return n + 1;
    }

    main() {
      var z;
      z = inc(1);
      return z;
    }
  )");

  auto cfgs = IntraproceduralCFGs::build(ast.get());
  auto graphs = cfgs->getAll();
  REQUIRE(graphs.size() == 2);

  for (auto *g : graphs) {
    REQUIRE_NOTHROW(g->validate());
  }
}

TEST_CASE("CFGBuilder: if without else has true branch and fallthrough false branch",
          "[cfg][CFGBuilder]") {
  auto ast = parseProgram(R"(
    main() {
      var x;
      x = 1;
      if (x) {
        output x;
      }
      x = x + 1;
      return x;
    }
  )");

  auto cfgs = IntraproceduralCFGs::build(ast.get());
  auto *f = ast->findFunctionByName("main");
  REQUIRE(f != nullptr);
  const auto &cfg = cfgs->get(f);

  REQUIRE_NOTHROW(cfg.validate());

  const auto *ifBlock = findFirstIfBlock(cfg);
  REQUIRE(ifBlock != nullptr);
  REQUIRE(ifBlock->getTerminatorKind() == CFGTerminatorKind::If);
  REQUIRE(ifBlock->getSuccessors().size() == 2);

  int trueEdgeCount = 0;
  int falseEdgeCount = 0;
  BlockId falseTarget = 0;
  for (const auto &edge : ifBlock->getSuccessors()) {
    if (edge.kind == CFGEdgeKind::TrueBranch) {
      trueEdgeCount++;
    }
    if (edge.kind == CFGEdgeKind::FalseBranch) {
      falseEdgeCount++;
      falseTarget = edge.target;
    }
  }
  REQUIRE(trueEdgeCount == 1);
  REQUIRE(falseEdgeCount == 1);

  const auto *contBlock = cfg.findBlock(falseTarget);
  REQUIRE(contBlock != nullptr);
  REQUIRE(contBlock->getTerminatorKind() == CFGTerminatorKind::Return);
}

TEST_CASE("CFGBuilder: if with else joins normal branches at continuation",
          "[cfg][CFGBuilder]") {
  auto ast = parseProgram(R"(
    main() {
      var x;
      x = 1;
      if (x) {
        output x;
      } else {
        error x;
      }
      x = x + 1;
      return x;
    }
  )");

  auto cfgs = IntraproceduralCFGs::build(ast.get());
  auto *f = ast->findFunctionByName("main");
  REQUIRE(f != nullptr);
  const auto &cfg = cfgs->get(f);

  REQUIRE_NOTHROW(cfg.validate());

  const auto *ifBlock = findFirstIfBlock(cfg);
  REQUIRE(ifBlock != nullptr);
  REQUIRE(ifBlock->getTerminatorKind() == CFGTerminatorKind::If);

  BlockId thenId = 0;
  BlockId elseId = 0;
  for (const auto &edge : ifBlock->getSuccessors()) {
    if (edge.kind == CFGEdgeKind::TrueBranch) {
      thenId = edge.target;
    } else if (edge.kind == CFGEdgeKind::FalseBranch) {
      elseId = edge.target;
    }
  }

  const auto *thenBlock = cfg.findBlock(thenId);
  const auto *elseBlock = cfg.findBlock(elseId);
  REQUIRE(thenBlock != nullptr);
  REQUIRE(elseBlock != nullptr);
  REQUIRE(thenBlock->getSuccessors().size() == 1);
  REQUIRE(elseBlock->getSuccessors().size() == 1);
  REQUIRE(thenBlock->getSuccessors()[0].target == elseBlock->getSuccessors()[0].target);
}

TEST_CASE("CFGBuilder: nested if preserves enclosing continuation",
          "[cfg][CFGBuilder]") {
  auto ast = parseProgram(R"(
    main() {
      var x;
      x = 1;
      if (x) {
        if (x) {
          output x;
        }
      }
      x = x + 1;
      return x;
    }
  )");

  auto cfgs = IntraproceduralCFGs::build(ast.get());
  auto *f = ast->findFunctionByName("main");
  REQUIRE(f != nullptr);
  const auto &cfg = cfgs->get(f);

  REQUIRE_NOTHROW(cfg.validate());

  auto stmts = f->getStmts();
  REQUIRE(stmts.size() >= 3);
  auto *outerIfStmt = dynamic_cast<ASTIfStmt *>(stmts[1]);
  REQUIRE(outerIfStmt != nullptr);

  auto *outerThenBlock = dynamic_cast<ASTBlockStmt *>(outerIfStmt->getThen());
  REQUIRE(outerThenBlock != nullptr);
  auto thenStmts = outerThenBlock->getStmts();
  REQUIRE(thenStmts.size() == 1);
  auto *innerIfStmt = dynamic_cast<ASTIfStmt *>(thenStmts[0]);
  REQUIRE(innerIfStmt != nullptr);

  const auto *outerIf = findIfBlockForStmt(cfg, outerIfStmt);
  REQUIRE(outerIf != nullptr);
  REQUIRE(outerIf->getTerminatorKind() == CFGTerminatorKind::If);

  BlockId outerFalse = 0;
  BlockId outerTrue = 0;
  for (const auto &edge : outerIf->getSuccessors()) {
    if (edge.kind == CFGEdgeKind::TrueBranch) {
      outerTrue = edge.target;
    } else if (edge.kind == CFGEdgeKind::FalseBranch) {
      outerFalse = edge.target;
    }
  }

  const auto *innerIf = findIfBlockForStmt(cfg, innerIfStmt);
  REQUIRE(innerIf != nullptr);
  REQUIRE(innerIf->getTerminatorKind() == CFGTerminatorKind::If);

  BlockId innerFalse = 0;
  for (const auto &edge : innerIf->getSuccessors()) {
    if (edge.kind == CFGEdgeKind::FalseBranch) {
      innerFalse = edge.target;
    }
  }

  REQUIRE(innerFalse == outerFalse);
}

TEST_CASE("CFGBuilder: conditional source locations belong to terminator block",
          "[cfg][CFGBuilder]") {
  auto ast = parseProgram(R"(
    main() {
      var x;
      x = 1;
      if (x) {
        output x;
      }
      return x;
    }
  )");

  auto cfgs = IntraproceduralCFGs::build(ast.get());
  auto *f = ast->findFunctionByName("main");
  REQUIRE(f != nullptr);
  const auto &cfg = cfgs->get(f);

  auto stmts = f->getStmts();
  REQUIRE(stmts.size() >= 3);
  auto *ifStmt = dynamic_cast<ASTIfStmt *>(stmts[1]);
  REQUIRE(ifStmt != nullptr);

  const auto *ifBlock = findFirstIfBlock(cfg);
  REQUIRE(ifBlock != nullptr);
  REQUIRE(ifBlock->getTerminatorStatement() == ifStmt);
  REQUIRE(ifBlock->getCondition() == ifStmt->getCondition());
}

TEST_CASE("CFGBuilder: while condition has true body edge and false continuation edge",
          "[cfg][CFGBuilder]") {
  auto ast = parseProgram(R"(
    main() {
      var x;
      x = 3;
      while (x) {
        x = x - 1;
      }
      return x;
    }
  )");

  auto cfgs = IntraproceduralCFGs::build(ast.get());
  auto *f = ast->findFunctionByName("main");
  REQUIRE(f != nullptr);
  const auto &cfg = cfgs->get(f);

  REQUIRE_NOTHROW(cfg.validate());

  auto *whileStmt = findFirstWhileStmt(f);
  REQUIRE(whileStmt != nullptr);

  const auto *whileBlock = findWhileBlockForStmt(cfg, whileStmt);
  REQUIRE(whileBlock != nullptr);
  REQUIRE(whileBlock->getTerminatorKind() == CFGTerminatorKind::While);
  REQUIRE(whileBlock->getTerminatorStatement() == whileStmt);
  REQUIRE(whileBlock->getCondition() == whileStmt->getCondition());

  int trueEdgeCount = 0;
  int falseEdgeCount = 0;
  BlockId falseTarget = 0;
  for (const auto &edge : whileBlock->getSuccessors()) {
    if (edge.kind == CFGEdgeKind::TrueBranch) {
      trueEdgeCount++;
    }
    if (edge.kind == CFGEdgeKind::FalseBranch) {
      falseEdgeCount++;
      falseTarget = edge.target;
    }
  }

  REQUIRE(trueEdgeCount == 1);
  REQUIRE(falseEdgeCount == 1);

  const auto *contBlock = cfg.findBlock(falseTarget);
  REQUIRE(contBlock != nullptr);
  REQUIRE(contBlock->getTerminatorKind() == CFGTerminatorKind::Return);
}

TEST_CASE("CFGBuilder: normal loop body back edge returns to condition",
          "[cfg][CFGBuilder]") {
  auto ast = parseProgram(R"(
    main() {
      var x;
      x = 2;
      while (x) {
        x = x - 1;
      }
      return x;
    }
  )");

  auto cfgs = IntraproceduralCFGs::build(ast.get());
  auto *f = ast->findFunctionByName("main");
  REQUIRE(f != nullptr);
  const auto &cfg = cfgs->get(f);

  REQUIRE_NOTHROW(cfg.validate());

  auto *whileStmt = findFirstWhileStmt(f);
  REQUIRE(whileStmt != nullptr);

  const auto *whileBlock = findWhileBlockForStmt(cfg, whileStmt);
  REQUIRE(whileBlock != nullptr);

  BlockId bodyTarget = 0;
  for (const auto &edge : whileBlock->getSuccessors()) {
    if (edge.kind == CFGEdgeKind::TrueBranch) {
      bodyTarget = edge.target;
    }
  }
  REQUIRE(bodyTarget != 0);

  const auto *bodyBlock = cfg.findBlock(bodyTarget);
  REQUIRE(bodyBlock != nullptr);
  REQUIRE(bodyBlock->getTerminatorKind() == CFGTerminatorKind::Fallthrough);
  REQUIRE(bodyBlock->getSuccessors().size() == 1);
  REQUIRE(bodyBlock->getSuccessors()[0].kind == CFGEdgeKind::Fallthrough);
  REQUIRE(bodyBlock->getSuccessors()[0].target == whileBlock->getId());
}

TEST_CASE("CFGBuilder: loop body with internal branch still rejoins condition",
          "[cfg][CFGBuilder]") {
  auto ast = parseProgram(R"(
    main() {
      var x;
      x = 2;
      while (x) {
        if (x) {
          output x;
        }
        x = x - 1;
      }
      return x;
    }
  )");

  auto cfgs = IntraproceduralCFGs::build(ast.get());
  auto *f = ast->findFunctionByName("main");
  REQUIRE(f != nullptr);
  const auto &cfg = cfgs->get(f);

  REQUIRE_NOTHROW(cfg.validate());

  auto *whileStmt = findFirstWhileStmt(f);
  REQUIRE(whileStmt != nullptr);

  const auto *whileBlock = findWhileBlockForStmt(cfg, whileStmt);
  REQUIRE(whileBlock != nullptr);

  BlockId bodyTarget = 0;
  for (const auto &edge : whileBlock->getSuccessors()) {
    if (edge.kind == CFGEdgeKind::TrueBranch) {
      bodyTarget = edge.target;
    }
  }
  REQUIRE(bodyTarget != 0);

  const auto *bodyEntry = cfg.findBlock(bodyTarget);
  REQUIRE(bodyEntry != nullptr);

  bool hasBackEdgePred = false;
  for (auto predId : whileBlock->getPredecessors()) {
    if (predId == whileBlock->getId()) {
      continue;
    }
    const auto *pred = cfg.findBlock(predId);
    if (pred == nullptr) {
      continue;
    }
    for (const auto &edge : pred->getSuccessors()) {
      if (edge.kind == CFGEdgeKind::Fallthrough &&
          edge.target == whileBlock->getId()) {
        hasBackEdgePred = true;
      }
    }
  }

  REQUIRE(hasBackEdgePred);
}

TEST_CASE("CFGBuilder: loop body return reaches exit without back edge",
          "[cfg][CFGBuilder]") {
  auto ast = parseProgram(R"(
    main() {
      var x;
      x = 2;
      while (x) {
        return x;
      }
      return x;
    }
  )");

  auto cfgs = IntraproceduralCFGs::build(ast.get());
  auto *f = ast->findFunctionByName("main");
  REQUIRE(f != nullptr);
  const auto &cfg = cfgs->get(f);

  REQUIRE_NOTHROW(cfg.validate());

  auto *whileStmt = findFirstWhileStmt(f);
  REQUIRE(whileStmt != nullptr);

  const auto *whileBlock = findWhileBlockForStmt(cfg, whileStmt);
  REQUIRE(whileBlock != nullptr);

  BlockId bodyTarget = 0;
  for (const auto &edge : whileBlock->getSuccessors()) {
    if (edge.kind == CFGEdgeKind::TrueBranch) {
      bodyTarget = edge.target;
    }
  }
  REQUIRE(bodyTarget != 0);

  const auto *bodyBlock = cfg.findBlock(bodyTarget);
  REQUIRE(bodyBlock != nullptr);
  REQUIRE(bodyBlock->getTerminatorKind() == CFGTerminatorKind::Return);
  REQUIRE(bodyBlock->getSuccessors().size() == 1);
  REQUIRE(bodyBlock->getSuccessors()[0].kind == CFGEdgeKind::ReturnToExit);
  REQUIRE(bodyBlock->getSuccessors()[0].target == cfg.getExit().getId());

  for (const auto &edge : bodyBlock->getSuccessors()) {
    REQUIRE(edge.target != whileBlock->getId());
  }
}

TEST_CASE("CFGBuilder: nested loops retain separate condition blocks",
          "[cfg][CFGBuilder]") {
  auto ast = parseProgram(R"(
    main() {
      var x, y;
      x = 2;
      y = 2;
      while (x) {
        while (y) {
          y = y - 1;
        }
        x = x - 1;
      }
      return x;
    }
  )");

  auto cfgs = IntraproceduralCFGs::build(ast.get());
  auto *f = ast->findFunctionByName("main");
  REQUIRE(f != nullptr);
  const auto &cfg = cfgs->get(f);

  REQUIRE_NOTHROW(cfg.validate());

  auto *outerWhileStmt = findFirstWhileStmt(f);
  REQUIRE(outerWhileStmt != nullptr);

  auto *outerBody = dynamic_cast<ASTBlockStmt *>(outerWhileStmt->getBody());
  REQUIRE(outerBody != nullptr);
  auto outerBodyStmts = outerBody->getStmts();
  REQUIRE(outerBodyStmts.size() >= 1);
  auto *innerWhileStmt = dynamic_cast<ASTWhileStmt *>(outerBodyStmts[0]);
  REQUIRE(innerWhileStmt != nullptr);

  const auto *outerWhileBlock = findWhileBlockForStmt(cfg, outerWhileStmt);
  const auto *innerWhileBlock = findWhileBlockForStmt(cfg, innerWhileStmt);
  REQUIRE(outerWhileBlock != nullptr);
  REQUIRE(innerWhileBlock != nullptr);
  REQUIRE(outerWhileBlock->getId() != innerWhileBlock->getId());
}

TEST_CASE("CFGBuilder: loop following straight-line statement reaches continuation",
          "[cfg][CFGBuilder]") {
  auto ast = parseProgram(R"(
    main() {
      var x;
      x = 2;
      while (x) {
        x = x - 1;
      }
      output x;
      return x;
    }
  )");

  auto cfgs = IntraproceduralCFGs::build(ast.get());
  auto *f = ast->findFunctionByName("main");
  REQUIRE(f != nullptr);
  const auto &cfg = cfgs->get(f);

  REQUIRE_NOTHROW(cfg.validate());

  auto stmts = f->getStmts();
  REQUIRE(stmts.size() >= 4);
  auto *whileStmt = findFirstWhileStmt(f);
  REQUIRE(whileStmt != nullptr);

  const auto *whileBlock = findWhileBlockForStmt(cfg, whileStmt);
  REQUIRE(whileBlock != nullptr);

  BlockId falseTarget = 0;
  for (const auto &edge : whileBlock->getSuccessors()) {
    if (edge.kind == CFGEdgeKind::FalseBranch) {
      falseTarget = edge.target;
    }
  }
  REQUIRE(falseTarget != 0);

  const auto *contBlock = cfg.findBlock(falseTarget);
  REQUIRE(contBlock != nullptr);
  REQUIRE(contBlock->getTerminatorKind() == CFGTerminatorKind::Return);
  REQUIRE(contBlock->getStatements().size() == 1);
  REQUIRE(contBlock->getStatements()[0] == stmts[2]);
}

TEST_CASE("CFGBuilder: case dispatch has one edge per arm in source order",
          "[cfg][CFGBuilder]") {
  auto ast = parseProgram(R"(
    type Opt = Some(val) | None;

    main() {
      var o, x;
      o = Some(1);
      case o of {
        Some(v) -> x = v;
        None -> x = 0;
      }
      return x;
    }
  )");

  auto cfgs = IntraproceduralCFGs::build(ast.get());
  auto *f = ast->findFunctionByName("main");
  REQUIRE(f != nullptr);
  const auto &cfg = cfgs->get(f);

  REQUIRE_NOTHROW(cfg.validate());

  auto *caseStmt = findFirstCaseStmt(f);
  REQUIRE(caseStmt != nullptr);
  auto arms = caseStmt->getArms();
  REQUIRE(arms.size() == 2);

  const auto *caseBlock = findCaseBlockForStmt(cfg, caseStmt);
  REQUIRE(caseBlock != nullptr);
  REQUIRE(caseBlock->getSuccessors().size() == arms.size());

  for (std::size_t i = 0; i < arms.size(); i++) {
    REQUIRE(caseBlock->getSuccessors()[i].kind == CFGEdgeKind::CaseArm);
  }
}

TEST_CASE("CFGBuilder: case arm edges retain stable pattern labels",
          "[cfg][CFGBuilder]") {
  auto ast = parseProgram(R"(
    type Opt = Some(val) | None;

    main() {
      var o, x;
      o = Some(1);
      case o of {
        Some(v) -> x = v;
        None -> x = 0;
      }
      return x;
    }
  )");

  auto cfgs = IntraproceduralCFGs::build(ast.get());
  auto *f = ast->findFunctionByName("main");
  REQUIRE(f != nullptr);
  const auto &cfg = cfgs->get(f);

  auto *caseStmt = findFirstCaseStmt(f);
  REQUIRE(caseStmt != nullptr);

  const auto *caseBlock = findCaseBlockForStmt(cfg, caseStmt);
  REQUIRE(caseBlock != nullptr);
  REQUIRE(caseBlock->getSuccessors().size() == 2);

  REQUIRE(caseBlock->getSuccessors()[0].label == "Some(v)");
  REQUIRE(caseBlock->getSuccessors()[1].label == "None");
}

TEST_CASE("CFGBuilder: normally completing case arms join continuation",
          "[cfg][CFGBuilder]") {
  auto ast = parseProgram(R"(
    type Opt = Some(val) | None;

    main() {
      var o, x;
      o = Some(1);
      case o of {
        Some(v) -> x = v;
        None -> x = 0;
      }
      output x;
      return x;
    }
  )");

  auto cfgs = IntraproceduralCFGs::build(ast.get());
  auto *f = ast->findFunctionByName("main");
  REQUIRE(f != nullptr);
  const auto &cfg = cfgs->get(f);

  REQUIRE_NOTHROW(cfg.validate());

  auto *caseStmt = findFirstCaseStmt(f);
  REQUIRE(caseStmt != nullptr);

  const auto *caseBlock = findCaseBlockForStmt(cfg, caseStmt);
  REQUIRE(caseBlock != nullptr);
  REQUIRE(caseBlock->getSuccessors().size() == 2);

  BlockId firstJoin = 0;
  for (const auto &edge : caseBlock->getSuccessors()) {
    const auto *armBlock = cfg.findBlock(edge.target);
    REQUIRE(armBlock != nullptr);
    REQUIRE(armBlock->getSuccessors().size() == 1);
    REQUIRE(armBlock->getSuccessors()[0].kind == CFGEdgeKind::Fallthrough);

    if (firstJoin == 0) {
      firstJoin = armBlock->getSuccessors()[0].target;
    }
    REQUIRE(armBlock->getSuccessors()[0].target == firstJoin);
  }
}

TEST_CASE("CFGBuilder: case dispatch stores scrutinee and source stmt",
          "[cfg][CFGBuilder]") {
  auto ast = parseProgram(R"(
    type Opt = Some(val) | None;

    main() {
      var o, x;
      o = Some(1);
      case o of {
        Some(v) -> x = v;
        None -> x = 0;
      }
      return x;
    }
  )");

  auto cfgs = IntraproceduralCFGs::build(ast.get());
  auto *f = ast->findFunctionByName("main");
  REQUIRE(f != nullptr);
  const auto &cfg = cfgs->get(f);

  auto *caseStmt = findFirstCaseStmt(f);
  REQUIRE(caseStmt != nullptr);

  const auto *caseBlock = findCaseBlockForStmt(cfg, caseStmt);
  REQUIRE(caseBlock != nullptr);
  REQUIRE(caseBlock->getTerminatorStatement() == caseStmt);
  REQUIRE(caseBlock->getCondition() == caseStmt->getScrutinee());
}

TEST_CASE("CFGBuilder: nested case preserves outer continuation",
          "[cfg][CFGBuilder]") {
  auto ast = parseProgram(R"(
    type Opt = Some(val) | None;

    main() {
      var o, x;
      o = Some(1);
      case o of {
        Some(v) -> case o of {
          Some(w) -> x = w;
          None -> x = 1;
        }
        None -> x = 0;
      }
      output x;
      return x;
    }
  )");

  auto cfgs = IntraproceduralCFGs::build(ast.get());
  auto *f = ast->findFunctionByName("main");
  REQUIRE(f != nullptr);
  const auto &cfg = cfgs->get(f);

  REQUIRE_NOTHROW(cfg.validate());

  auto stmts = f->getStmts();
  REQUIRE(stmts.size() >= 4);
  auto *outerCase = dynamic_cast<ASTCaseStmt *>(stmts[1]);
  REQUIRE(outerCase != nullptr);

  auto outerArms = outerCase->getArms();
  REQUIRE(outerArms.size() == 2);
  auto *innerCase = dynamic_cast<ASTCaseStmt *>(outerArms[0]->getBody());
  REQUIRE(innerCase != nullptr);

  const auto *outerCaseBlock = findCaseBlockForStmt(cfg, outerCase);
  const auto *innerCaseBlock = findCaseBlockForStmt(cfg, innerCase);
  REQUIRE(outerCaseBlock != nullptr);
  REQUIRE(innerCaseBlock != nullptr);

  BlockId outerJoin = 0;
  for (const auto &edge : outerCaseBlock->getSuccessors()) {
    if (edge.label != "None") {
      continue;
    }
    const auto *noneArmBlock = cfg.findBlock(edge.target);
    REQUIRE(noneArmBlock != nullptr);
    REQUIRE(noneArmBlock->getSuccessors().size() == 1);
    REQUIRE(noneArmBlock->getSuccessors()[0].kind == CFGEdgeKind::Fallthrough);
    outerJoin = noneArmBlock->getSuccessors()[0].target;
  }
  REQUIRE(outerJoin != 0);

  BlockId innerJoin = 0;
  for (const auto &edge : innerCaseBlock->getSuccessors()) {
    const auto *armBlock = cfg.findBlock(edge.target);
    REQUIRE(armBlock != nullptr);
    REQUIRE(armBlock->getSuccessors().size() == 1);
    REQUIRE(armBlock->getSuccessors()[0].kind == CFGEdgeKind::Fallthrough);
    if (innerJoin == 0) {
      innerJoin = armBlock->getSuccessors()[0].target;
    }
  }

  REQUIRE(innerJoin == outerJoin);
}

TEST_CASE("CFGBuilder: returning case arm does not join continuation",
          "[cfg][CFGBuilder]") {
  auto ast = parseProgram(R"(
    type Opt = Some(val) | None;

    main() {
      var o, x;
      o = Some(1);
      case o of {
        Some(v) -> return v;
        None -> x = 0;
      }
      output x;
      return x;
    }
  )");

  auto cfgs = IntraproceduralCFGs::build(ast.get());
  auto *f = ast->findFunctionByName("main");
  REQUIRE(f != nullptr);
  const auto &cfg = cfgs->get(f);

  REQUIRE_NOTHROW(cfg.validate());

  auto *caseStmt = findFirstCaseStmt(f);
  REQUIRE(caseStmt != nullptr);

  const auto *caseBlock = findCaseBlockForStmt(cfg, caseStmt);
  REQUIRE(caseBlock != nullptr);
  REQUIRE(caseBlock->getSuccessors().size() == 2);

  const BasicBlock *someArm = nullptr;
  const BasicBlock *noneArm = nullptr;
  for (const auto &edge : caseBlock->getSuccessors()) {
    if (edge.label == "Some(v)") {
      someArm = cfg.findBlock(edge.target);
    } else if (edge.label == "None") {
      noneArm = cfg.findBlock(edge.target);
    }
  }

  REQUIRE(someArm != nullptr);
  REQUIRE(noneArm != nullptr);

  REQUIRE(someArm->getTerminatorKind() == CFGTerminatorKind::Return);
  REQUIRE(someArm->getSuccessors().size() == 1);
  REQUIRE(someArm->getSuccessors()[0].kind == CFGEdgeKind::ReturnToExit);
  REQUIRE(someArm->getSuccessors()[0].target == cfg.getExit().getId());

  REQUIRE(noneArm->getSuccessors().size() == 1);
  REQUIRE(noneArm->getSuccessors()[0].kind == CFGEdgeKind::Fallthrough);
}

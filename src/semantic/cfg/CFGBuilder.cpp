#include "CFGBuilder.h"

#include "IntraproceduralCFGs.h"

#include "ASTBlockStmt.h"
#include "ASTCaseStmt.h"
#include "ASTFunction.h"
#include "ASTIfStmt.h"
#include "ASTProgram.h"
#include "ASTReturnStmt.h"
#include "ASTWhileStmt.h"
#include "InternalError.h"

#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {
constexpr BlockId kUnresolvedExit = std::numeric_limits<BlockId>::max();

struct BlockDraft {
  BlockId id;
  std::string name;
  std::vector<const ASTStmt *> statements;
  CFGTerminatorKind terminatorKind;
  const ASTStmt *terminatorStatement;
  const ASTExpr *condition;
  std::vector<CFGEdge> successors;
};

class FunctionCFGBuilder {
public:
  explicit FunctionCFGBuilder(ASTFunction *function)
      : function(function), nextId(1) {}

  ControlFlowGraph build() {
    auto stmts = function->getStmts();
    if (stmts.empty()) {
      return buildEmpty();
    }

    auto *ret = dynamic_cast<ASTReturnStmt *>(stmts.back());
    if (ret == nullptr) {
      throw InternalError("CFGBuilder expects final statement to be a return");
    }

    BlockId entryTarget = buildSequence(stmts, kUnresolvedExit);

    BlockId exitId = allocateId();
    drafts.push_back({exitId,
                      "exit",
                      {},
                      CFGTerminatorKind::Fallthrough,
                      nullptr,
                      nullptr,
                      {}});

    for (auto &draft : drafts) {
      for (auto &edge : draft.successors) {
        if (edge.kind == CFGEdgeKind::ReturnToExit &&
            edge.target == kUnresolvedExit) {
          edge.target = exitId;
        }
      }
    }

    std::vector<BlockDraft> ordered;
    ordered.reserve(drafts.size() + 1);
    ordered.push_back({0,
                       "entry",
                       {},
                       CFGTerminatorKind::Fallthrough,
                       nullptr,
                       nullptr,
                       {{entryTarget, CFGEdgeKind::Fallthrough, ""}}});
    for (auto &draft : drafts) {
      ordered.push_back(std::move(draft));
    }

    std::unordered_map<BlockId, std::vector<BlockId>> preds;
    for (const auto &draft : ordered) {
      preds.try_emplace(draft.id, std::vector<BlockId>{});
    }
    for (const auto &draft : ordered) {
      for (const auto &edge : draft.successors) {
        preds[edge.target].push_back(draft.id);
      }
    }

    std::vector<BasicBlock> blocks;
    blocks.reserve(ordered.size());
    for (auto &draft : ordered) {
      blocks.emplace_back(draft.id, std::move(draft.name),
                          std::move(draft.statements), draft.terminatorKind,
                          draft.terminatorStatement, draft.condition,
                          std::move(draft.successors),
                          std::move(preds[draft.id]));
    }

    return ControlFlowGraph(function, std::move(blocks), 0, exitId);
  }

private:
  ASTFunction *function;
  BlockId nextId;
  std::vector<BlockDraft> drafts;

  BlockId allocateId() { return nextId++; }

  std::string formatCaseArmLabel(const ASTCaseArm *arm) {
    std::string label = arm->getTag();
    auto patterns = arm->getPatterns();
    if (patterns.empty()) {
      return label;
    }

    label += "(";
    for (std::size_t i = 0; i < patterns.size(); i++) {
      if (i > 0) {
        label += ", ";
      }
      std::stringstream rendered;
      rendered << *patterns[i];
      label += rendered.str();
    }
    label += ")";
    return label;
  }

  ControlFlowGraph buildEmpty() {
    std::vector<BasicBlock> blocks;

    blocks.emplace_back(0, "entry", std::vector<const ASTStmt *>{},
                        CFGTerminatorKind::Fallthrough, nullptr, nullptr,
                        std::vector<CFGEdge>{{1, CFGEdgeKind::Fallthrough, ""}},
                        std::vector<BlockId>{});

    blocks.emplace_back(1, "exit", std::vector<const ASTStmt *>{},
                        CFGTerminatorKind::Fallthrough, nullptr, nullptr,
                        std::vector<CFGEdge>{}, std::vector<BlockId>{0});

    return ControlFlowGraph(function, std::move(blocks), 0, 1);
  }

  BlockId createBlock(std::vector<const ASTStmt *> statements,
                      CFGTerminatorKind terminatorKind,
                      const ASTStmt *terminatorStatement,
                      const ASTExpr *condition,
                      std::vector<CFGEdge> successors) {
    BlockId id = allocateId();
    drafts.push_back({id,
                      "b" + std::to_string(id - 1),
                      std::move(statements),
                      terminatorKind,
                      terminatorStatement,
                      condition,
                      std::move(successors)});
    return id;
  }

  BlockId buildStmt(ASTStmt *stmt, BlockId continuation) {
    if (auto *ret = dynamic_cast<ASTReturnStmt *>(stmt)) {
      return createBlock({}, CFGTerminatorKind::Return, ret, nullptr,
                         {{kUnresolvedExit, CFGEdgeKind::ReturnToExit,
                           "return"}});
    }

    if (auto *ifStmt = dynamic_cast<ASTIfStmt *>(stmt)) {
      BlockId thenEntry = buildStmt(ifStmt->getThen(), continuation);
      BlockId elseEntry = continuation;
      if (ifStmt->getElse() != nullptr) {
        elseEntry = buildStmt(ifStmt->getElse(), continuation);
      }

      return createBlock({}, CFGTerminatorKind::If, ifStmt,
                         ifStmt->getCondition(),
                         {{thenEntry, CFGEdgeKind::TrueBranch, ""},
                          {elseEntry, CFGEdgeKind::FalseBranch, ""}});
    }

    if (auto *whileStmt = dynamic_cast<ASTWhileStmt *>(stmt)) {
      BlockId whileId = allocateId();
      BlockId bodyEntry = buildStmt(whileStmt->getBody(), whileId);

      drafts.push_back({whileId,
                        "b" + std::to_string(whileId - 1),
                        {},
                        CFGTerminatorKind::While,
                        whileStmt,
                        whileStmt->getCondition(),
                        {{bodyEntry, CFGEdgeKind::TrueBranch, ""},
                         {continuation, CFGEdgeKind::FalseBranch, ""}}});

      return whileId;
    }

    if (auto *caseStmt = dynamic_cast<ASTCaseStmt *>(stmt)) {
      auto arms = caseStmt->getArms();
      if (arms.empty()) {
        throw InternalError("CFGBuilder encountered case statement with no arms");
      }

      std::vector<CFGEdge> armEdges;
      armEdges.reserve(arms.size());

      for (auto *arm : arms) {
        BlockId armEntry = buildStmt(arm->getBody(), continuation);
        armEdges.push_back(
            {armEntry, CFGEdgeKind::CaseArm, formatCaseArmLabel(arm)});
      }

      return createBlock({}, CFGTerminatorKind::Case, caseStmt,
                         caseStmt->getScrutinee(), std::move(armEdges));
    }

    if (auto *block = dynamic_cast<ASTBlockStmt *>(stmt)) {
      return buildSequence(block->getStmts(), continuation);
    }

    return createBlock({stmt}, CFGTerminatorKind::Fallthrough, nullptr, nullptr,
                       {{continuation, CFGEdgeKind::Fallthrough, ""}});
  }

  BlockId buildSequence(const std::vector<ASTStmt *> &stmts,
                        BlockId continuation) {
    if (stmts.empty()) {
      return continuation;
    }

    BlockId current = continuation;
    std::size_t i = stmts.size();
    for (std::size_t k = 0; k < stmts.size(); k++) {
      if (dynamic_cast<ASTReturnStmt *>(stmts[k]) != nullptr) {
        i = k + 1;
        break;
      }
    }

    while (i > 0) {
      std::size_t start = i;
      while (start > 0) {
        ASTStmt *candidate = stmts[start - 1];
        if (dynamic_cast<ASTIfStmt *>(candidate) != nullptr ||
            dynamic_cast<ASTWhileStmt *>(candidate) != nullptr ||
            dynamic_cast<ASTCaseStmt *>(candidate) != nullptr ||
            dynamic_cast<ASTBlockStmt *>(candidate) != nullptr) {
          break;
        }
        start--;
      }

      if (start < i) {
        auto *last = stmts[i - 1];
        if (auto *ret = dynamic_cast<ASTReturnStmt *>(last)) {
          std::vector<const ASTStmt *> prefix;
          prefix.reserve(i - start - 1);
          for (std::size_t k = start; k + 1 < i; k++) {
            prefix.push_back(stmts[k]);
          }

          current = createBlock(std::move(prefix), CFGTerminatorKind::Return,
                                ret, nullptr,
                                {{kUnresolvedExit, CFGEdgeKind::ReturnToExit,
                                  "return"}});
        } else {
          std::vector<const ASTStmt *> run;
          run.reserve(i - start);
          for (std::size_t k = start; k < i; k++) {
            run.push_back(stmts[k]);
          }

          current = createBlock(std::move(run), CFGTerminatorKind::Fallthrough,
                                nullptr, nullptr,
                                {{current, CFGEdgeKind::Fallthrough, ""}});
        }

        i = start;
        continue;
      }

      current = buildStmt(stmts[i - 1], current);
      i--;
    }

    return current;
  }
};
} // namespace

std::shared_ptr<IntraproceduralCFGs> CFGBuilder::build(ASTProgram *program) {
  if (program == nullptr) {
    throw InternalError("CFGBuilder::build received null ASTProgram");
  }

  std::vector<ControlFlowGraph> graphs;
  std::unordered_map<const ASTFunction *, std::size_t> byFunction;

  auto functions = program->getFunctions();
  graphs.reserve(functions.size());

  for (auto *function : functions) {
    byFunction[function] = graphs.size();
    auto cfg = buildForFunction(function);
    cfg.validate();
    graphs.push_back(std::move(cfg));
  }

  return std::make_shared<IntraproceduralCFGs>(std::move(graphs),
                                               std::move(byFunction));
}

ControlFlowGraph CFGBuilder::buildForFunction(ASTFunction *function) {
  if (function == nullptr) {
    throw InternalError("CFGBuilder encountered null ASTFunction");
  }

  FunctionCFGBuilder builder(function);
  return builder.build();
}

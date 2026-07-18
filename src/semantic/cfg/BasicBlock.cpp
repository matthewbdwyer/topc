#include "BasicBlock.h"

#include <utility>

BasicBlock::BasicBlock(BlockId id, std::string name,
                       std::vector<const ASTStmt *> statements,
                       CFGTerminatorKind terminatorKind,
                       const ASTStmt *terminatorStatement,
                       const ASTExpr *condition,
                       std::vector<CFGEdge> successors,
                       std::vector<BlockId> predecessors)
    : id(id), name(std::move(name)), statements(std::move(statements)),
      terminatorKind(terminatorKind),
      terminatorStatement(terminatorStatement), condition(condition),
      successors(std::move(successors)), predecessors(std::move(predecessors)) {}

BlockId BasicBlock::getId() const { return id; }

const std::string &BasicBlock::getName() const { return name; }

const std::vector<const ASTStmt *> &BasicBlock::getStatements() const {
  return statements;
}

CFGTerminatorKind BasicBlock::getTerminatorKind() const {
  return terminatorKind;
}

const ASTStmt *BasicBlock::getTerminatorStatement() const {
  return terminatorStatement;
}

const ASTExpr *BasicBlock::getCondition() const { return condition; }

const std::vector<CFGEdge> &BasicBlock::getSuccessors() const {
  return successors;
}

const std::vector<BlockId> &BasicBlock::getPredecessors() const {
  return predecessors;
}

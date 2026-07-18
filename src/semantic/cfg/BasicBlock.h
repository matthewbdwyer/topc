#pragma once

#include "CFGTypes.h"

#include <string>
#include <vector>

class ASTExpr;
class ASTStmt;

class BasicBlock {
public:
  BasicBlock(BlockId id, std::string name,
             std::vector<const ASTStmt *> statements,
             CFGTerminatorKind terminatorKind,
             const ASTStmt *terminatorStatement,
             const ASTExpr *condition,
             std::vector<CFGEdge> successors,
             std::vector<BlockId> predecessors);

  BlockId getId() const;
  const std::string &getName() const;
  const std::vector<const ASTStmt *> &getStatements() const;
  CFGTerminatorKind getTerminatorKind() const;
  const ASTStmt *getTerminatorStatement() const;
  const ASTExpr *getCondition() const;
  const std::vector<CFGEdge> &getSuccessors() const;
  const std::vector<BlockId> &getPredecessors() const;

private:
  BlockId id;
  std::string name;
  std::vector<const ASTStmt *> statements;
  CFGTerminatorKind terminatorKind;
  const ASTStmt *terminatorStatement;
  const ASTExpr *condition;
  std::vector<CFGEdge> successors;
  std::vector<BlockId> predecessors;
};

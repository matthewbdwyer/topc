#pragma once

#include "BasicBlock.h"

#include <vector>

class ASTFunction;

class ControlFlowGraph {
public:
  ControlFlowGraph(const ASTFunction *function, std::vector<BasicBlock> blocks,
                   BlockId entryId, BlockId exitId);

  const ASTFunction *getFunction() const;
  const BasicBlock &getEntry() const;
  const BasicBlock &getExit() const;
  const std::vector<BasicBlock> &getBlocks() const;
  const BasicBlock *findBlock(BlockId id) const;

  void validate() const;

private:
  const ASTFunction *function;
  std::vector<BasicBlock> blocks;
  BlockId entryId;
  BlockId exitId;
};

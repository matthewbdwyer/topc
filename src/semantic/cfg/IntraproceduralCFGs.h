#pragma once

#include "ControlFlowGraph.h"

#include <memory>
#include <unordered_map>
#include <vector>

class ASTFunction;
class ASTProgram;

class IntraproceduralCFGs {
public:
  static std::shared_ptr<IntraproceduralCFGs> build(ASTProgram *program);

  IntraproceduralCFGs(std::vector<ControlFlowGraph> graphs,
                      std::unordered_map<const ASTFunction *, std::size_t> byFunction);

  const ControlFlowGraph &get(const ASTFunction *function) const;
  std::vector<const ControlFlowGraph *> getAll() const;

private:
  std::vector<ControlFlowGraph> graphs;
  std::unordered_map<const ASTFunction *, std::size_t> byFunction;
};

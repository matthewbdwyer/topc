#include "IntraproceduralCFGs.h"

#include "CFGBuilder.h"
#include "InternalError.h"

#include <sstream>
#include <utility>

std::shared_ptr<IntraproceduralCFGs> IntraproceduralCFGs::build(
    ASTProgram *program) {
  return CFGBuilder::build(program);
}

IntraproceduralCFGs::IntraproceduralCFGs(
    std::vector<ControlFlowGraph> graphs,
    std::unordered_map<const ASTFunction *, std::size_t> byFunction)
    : graphs(std::move(graphs)), byFunction(std::move(byFunction)) {}

const ControlFlowGraph &IntraproceduralCFGs::get(const ASTFunction *function) const {
  auto it = byFunction.find(function);
  if (it == byFunction.end()) {
    throw InternalError("CFG lookup failed for unknown function");
  }
  return graphs.at(it->second);
}

std::vector<const ControlFlowGraph *> IntraproceduralCFGs::getAll() const {
  std::vector<const ControlFlowGraph *> out;
  out.reserve(graphs.size());
  for (const auto &graph : graphs) {
    out.push_back(&graph);
  }
  return out;
}

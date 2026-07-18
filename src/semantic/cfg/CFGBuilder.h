#pragma once

#include <memory>

class ASTFunction;
class ASTProgram;
class ControlFlowGraph;
class IntraproceduralCFGs;

class CFGBuilder {
public:
  static std::shared_ptr<IntraproceduralCFGs> build(ASTProgram *program);

private:
  static ControlFlowGraph buildForFunction(ASTFunction *function);
};

#include "AST.h"
#include "CodeGenVisitor.h"
#include "SemanticAnalysis.h"

#include <memory>
#include <string>

std::shared_ptr<llvm::Module>
ASTProgram::codegen(SemanticAnalysis *semanticAnalysis,
                    const std::string &programName) {
  CodeGenVisitor visitor;
  return visitor.generate(this, semanticAnalysis, programName);
}

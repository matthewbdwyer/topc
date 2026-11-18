#pragma once

// A TOP program, parsed and checked, for tests of the stages that read the AST.
#include "ASTHelper.h"
#include "Encoding.h"
#include "SemanticAnalysis.h"

#include <sstream>

namespace vtest {
struct Program {
  std::shared_ptr<ASTProgram> ast;
  std::shared_ptr<SemanticAnalysis> sem;
  verutop::Encoding enc;

  explicit Program(const std::string &source) {
    std::stringstream stream(source);
    ast = ASTHelper::build_ast(stream);
    sem = SemanticAnalysis::analyze(ast.get());
    enc.build(ast.get(), sem->getTypeResults());
  }
  ASTFunction *fn(const std::string &name) const {
    for (auto *f : ast->getFunctions())
      if (f->getName() == name) return f;
    return nullptr;
  }
};
} // namespace vtest

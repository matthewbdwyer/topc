#include "SemanticAnalysis.h"
#include "CheckAssignable.h"
#include "CheckBorrowPositions.h"
#include "CheckCaseCompleteness.h"
#include "CheckConstructorCase.h"
#include "CheckSumTypeNames.h"

std::shared_ptr<SemanticAnalysis> SemanticAnalysis::analyze(ASTProgram *ast,
                                                            bool polyInf) {
  auto symTable = SymbolTable::build(ast);
  CheckAssignable::check(ast);
  CheckBorrowPositions::check(ast);
  CheckSumTypeNames::check(ast);
  CheckCaseCompleteness::check(ast);
  CheckConstructorCase::check(ast);
  auto callGraph = CallGraph::build(ast, symTable.get());
  auto typeResults =
      TypeInference::run(ast, polyInf, callGraph.get(), symTable.get());
  return std::make_shared<SemanticAnalysis>(symTable, typeResults, callGraph);
}

SymbolTable *SemanticAnalysis::getSymbolTable() { return symTable.get(); };

TypeInference *SemanticAnalysis::getTypeResults() { return typeResults.get(); };

CallGraph *SemanticAnalysis::getCallGraph() { return callGraph.get(); };

#include "SemanticAnalysis.h"
#include "BorrowChecker.h"
#include "CheckAssignable.h"
#include "CheckBorrowPositions.h"
#include "CheckCaseCompleteness.h"
#include "CheckConstructorCase.h"
#include "CheckPatternTypes.h"
#include "CheckRecordFields.h"
#include "CheckSumTypeNames.h"
#include "DestructionPass.h"
#include "MoveAnalysis.h"
#include "OwnershipClassifier.h"

std::shared_ptr<SemanticAnalysis> SemanticAnalysis::analyze(ASTProgram *ast) {
  auto symTable = SymbolTable::build(ast);
  CheckAssignable::check(ast);
  CheckRecordFields::check(ast);
  CheckBorrowPositions::check(ast);
  BorrowChecker::check(ast);
  CheckSumTypeNames::check(ast);
  CheckCaseCompleteness::check(ast);
  CheckConstructorCase::check(ast);
  CheckPatternTypes::check(ast);

  // Build source-level CFGs before later semantic phases mutate the AST
  // (e.g., destruction insertion).
  auto intraproceduralCFGs = IntraproceduralCFGs::build(ast);

  auto callGraph = CallGraph::build(ast, symTable.get());
  auto typeResults =
      TypeInference::run(ast, callGraph.get(), symTable.get());
  auto ownershipClassifier = std::make_shared<OwnershipClassifier>(
      symTable.get(), typeResults.get());
  MoveAnalysis(ast, symTable.get(), ownershipClassifier.get());
  DestructionPass::run(ast, symTable.get(), ownershipClassifier.get());
  return std::make_shared<SemanticAnalysis>(symTable, intraproceduralCFGs,
                                            typeResults, callGraph,
                                            ownershipClassifier);
}

SymbolTable *SemanticAnalysis::getSymbolTable() { return symTable.get(); };

IntraproceduralCFGs *SemanticAnalysis::getIntraproceduralCFGs() {
  return intraproceduralCFGs.get();
};

TypeInference *SemanticAnalysis::getTypeResults() { return typeResults.get(); };

CallGraph *SemanticAnalysis::getCallGraph() { return callGraph.get(); };

OwnershipClassifier *SemanticAnalysis::getOwnershipClassifier() {
  return ownershipClassifier.get();
};

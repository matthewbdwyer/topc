#include "SemanticAnalysis.h"
#include "BorrowChecker.h"
#include "CheckAllocPayload.h"
#include "CheckAssignable.h"
#include "CheckBorrowPositions.h"
#include "CheckCaseCompleteness.h"
#include "CheckPatternTypes.h"
#include "CheckSumTypeNames.h"
#include "DestructionPass.h"
#include "MoveAnalysis.h"
#include "OwnershipClassifier.h"
#include "SemanticLogging.h"

std::shared_ptr<SemanticAnalysis> SemanticAnalysis::analyze(ASTProgram *ast) {
  SEMANTIC_LOG(1, "pipeline") << "start";
  auto symTable = SymbolTable::build(ast);
  CheckAssignable::check(ast);
  CheckBorrowPositions::check(ast);
  BorrowChecker::check(ast);
  CheckSumTypeNames::check(ast);
  CheckCaseCompleteness::check(ast);
  CheckPatternTypes::check(ast);

  // Build source-level CFGs before later semantic phases mutate the AST
  // (e.g., destruction insertion).
  auto intraproceduralCFGs = IntraproceduralCFGs::build(ast);
  auto callGraph = CallGraph::build(ast, symTable.get());
  auto typeResults = TypeInference::run(ast, callGraph.get(), symTable.get());
  CheckAllocPayload::check(ast, typeResults.get());
  auto ownershipClassifier = std::make_shared<OwnershipClassifier>(
      symTable.get(), typeResults.get());
  auto functionEffectSummaries = FunctionEffectSummaries::build(
      ast, symTable.get(), typeResults.get(), ownershipClassifier.get());
  BorrowChecker::checkInterprocedural(ast, symTable.get(),
                                      functionEffectSummaries.get());
  MoveAnalysis(ast, symTable.get(), ownershipClassifier.get(),
               functionEffectSummaries.get());
  DestructionPass::run(ast, symTable.get(), ownershipClassifier.get(),
                       functionEffectSummaries.get());
  SEMANTIC_LOG(1, "pipeline") << "complete";
  return std::make_shared<SemanticAnalysis>(symTable, intraproceduralCFGs,
                                            typeResults, callGraph,
                                            ownershipClassifier,
                                            functionEffectSummaries);
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

FunctionEffectSummaries *SemanticAnalysis::getFunctionEffectSummaries() {
  return functionEffectSummaries.get();
};

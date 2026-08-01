#pragma once

#include "ASTNode.h"
#include "ASTProgram.h"
#include "FunctionEffectSummaries.h"
#include "MoveAnalysis.h"
#include "OwnershipClassifier.h"
#include "SymbolTable.h"
#include "TypeInference.h"
#include "cfg/IntraproceduralCFGs.h"
#include "cfa/CallGraph.h" //call graph builder header
#include <memory>

/*! \class SemanticAnalysis
 *  \brief Stores the results of semantic analysis passes.
 *
 * This class provides the analyze method to run a set of semantic analyses,
 * including l-value checking for assignment statements, proper use of symbols,
 * and type checking and control flow analysis \sa SymbolTable \sa TypeInference
 * \sa CallGraph
 */
class SemanticAnalysis {
  std::shared_ptr<SymbolTable> symTable;
  std::shared_ptr<IntraproceduralCFGs> intraproceduralCFGs;
  std::shared_ptr<TypeInference> typeResults;
  std::shared_ptr<CallGraph> callGraph;
  std::shared_ptr<OwnershipClassifier> ownershipClassifier;
  std::shared_ptr<FunctionEffectSummaries> functionEffectSummaries;

public:
  SemanticAnalysis(std::shared_ptr<SymbolTable> s,
                   std::shared_ptr<IntraproceduralCFGs> cfgs,
                   std::shared_ptr<TypeInference> t,
                   std::shared_ptr<CallGraph> cg,
                   std::shared_ptr<OwnershipClassifier> oc,
                   std::shared_ptr<FunctionEffectSummaries> fe)
      : symTable(std::move(s)), intraproceduralCFGs(std::move(cfgs)),
        typeResults(std::move(t)),
        callGraph(std::move(cg)), ownershipClassifier(std::move(oc)),
        functionEffectSummaries(std::move(fe)) {}

  /*! \fn analyze
   *  \brief Perform semantic analysis on program AST.
   *
   * Run weeding, symbol, and type checking and control flow analysis.  Errors
   * in any of these result in a SemanticError.  If no errors then ownership of
   * semantic analysis results are transferred to caller. \sa SemanticError
   * \param ast The program AST
   * \return The unique pointer to the semantic analysis structure.
   */
  [[nodiscard]] static std::shared_ptr<SemanticAnalysis> analyze(ASTProgram *ast);

  /*! \fn getSymbolTable
   *  \brief Returns the symbol table computed for the program.
   * \sa SymbolTable
   */
  SymbolTable *getSymbolTable();

  /*! \fn getIntraproceduralCFGs
   *  \brief Returns the source-level intraprocedural CFG result.
   */
  IntraproceduralCFGs *getIntraproceduralCFGs();

  /*! \fn getTypeResults
   *  \brief Returns the type inference results.
   * \sa TypeInference
   */
  TypeInference *getTypeResults();

  /*! \fn getCallGraph
   *  \brief Returns the call graph for the program.
   * \sa CallGraph
   */
  CallGraph *getCallGraph();

  /*! \fn getOwnershipClassifier
   *  \brief Returns the ownership classifier result.
   * \sa OwnershipClassifier
   */
  OwnershipClassifier *getOwnershipClassifier();

  /*! \fn getFunctionEffectSummaries
   *  \brief Returns the inter-procedural function effect summaries.
   */
  FunctionEffectSummaries *getFunctionEffectSummaries();
};

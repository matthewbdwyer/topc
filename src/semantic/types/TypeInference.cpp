#include <cassert>
#include <unordered_set>
#include "TypeInference.h"
#include "ASTExpr.h"
#include "PolyTypeConstraintCollectVisitor.h"
#include "TopMu.h"
#include "TopSumType.h"
#include "Unifier.h"
#include "TypeConstraint.h"
#include "TypeConstraintCollectVisitor.h"
#include "../SemanticLogging.h"
#include <memory>

/* Local name space for DFS visit variables */
namespace {
std::deque<ASTFunction *> sorted;
std::vector<ASTFunction *> unmarked;
} // namespace

/* DFS to compute call dependence assuming that the graph
 * does not have cycles.  The DFS updates the unmarked vector
 * which records the functions whose dependences have been computed.
 */
void topoVisit(CallGraph *cg, ASTFunction *f) {
  // If f is marked, i.e., not in unmarked, then backtrack DFS
  if (std::find(unmarked.begin(), unmarked.end(), f) == unmarked.end()) {
    return;
  }

  // remove it from unmarked
  auto fPosition = std::find(unmarked.begin(), unmarked.end(), f);
  assert(fPosition != unmarked.end()); // f must be in the unmarked list
  unmarked.erase(fPosition);

  // visit called functions
  for (auto c : cg->getCallees(f)) {
    topoVisit(cg, c);
  }

  // add it to sorted
  sorted.push_back(f);
}

/* Determine whether there is a call chain from function f to g.
 * To determine if a function is recursive call with f==g.
 * Uses a visited set to avoid infinite recursion on cyclic call graphs.
 */
static bool mayIndirectlyCallHelper(CallGraph *cg, ASTFunction *f,
                                    ASTFunction *g,
                                    std::unordered_set<ASTFunction *> &visited) {
  if (!visited.insert(f).second)
    return false; // already explored from f
  for (auto c : cg->getCallees(f)) {
    if (c == g)
      return true;
    if (mayIndirectlyCallHelper(cg, c, g, visited))
      return true;
  }
  return false;
}

bool mayIndirectlyCall(CallGraph *cg, ASTFunction *f, ASTFunction *g) {
  std::unordered_set<ASTFunction *> visited;
  return mayIndirectlyCallHelper(cg, f, g, visited);
}

// Topologically sort the set of functions based on the call graph.
std::deque<ASTFunction *> topoSort(CallGraph *cg,
                                   std::vector<ASTFunction *> funcs) {
  /* Initialize globals for sort DFS */
  sorted = std::deque<ASTFunction *>();
  unmarked = funcs;

  while (!unmarked.empty()) {
    auto f = unmarked.back();
    topoVisit(cg, f);
  }
  return sorted;
}

std::vector<ASTFunction *> recursiveFuncs(CallGraph *cg) {
  auto funcs = cg->getVertices();
  auto recursiveFuncs = std::vector<ASTFunction *>();
  for (auto f : funcs) {
    if (mayIndirectlyCall(cg, f, f)) {
      recursiveFuncs.push_back(f);
    }
  }
  return recursiveFuncs;
}

/* Filters the call graph to eliminate any functions that call, either
 * directly or indirectly, a recursive function.
 * Returns a topological ordering of functions in the filtered graph.
 */
std::deque<ASTFunction *> topoSortNonRecursive(CallGraph *cg) {
  auto recFuncs = recursiveFuncs(cg);

  // Filter functions that directly or indirectly call a recursive function
  auto nonRecursiveFuncs = std::vector<ASTFunction *>();
  for (auto f : cg->getVertices()) {
    bool filter = false;
    for (auto r : recFuncs) {
      if (f == r) {
        filter = true;
      } else {
        filter = mayIndirectlyCall(cg, f, r);
      }
      if (filter)
        break;
    }

    if (!filter) {
      nonRecursiveFuncs.push_back(f);
    }
  }

  // Topologically sort the graph comprised of the non-recursive functions.
  return topoSort(cg, nonRecursiveFuncs);
}

/*
 * The returned unifier accounts for all of the program that is NOT
 * handled within the elements of the unifier map, i.e., is not
 * subjected to polymorphic type inference.
 */
std::shared_ptr<TypeInference> runPoly(ASTProgram *ast, SymbolTable *symbols,
                                       CallGraph *cg) {
  SEMANTIC_LOG(2, "type-inference") << "stage=polymorphic start";

  /* A single unifier is used for the staged polymorphic inference
   * and then the final monomorphic inference process.  The unifier
   * is solved after each stage, which corresponds to processing the
   * constraints of a non-recursive function.
   */
  auto unifier = std::make_shared<Unifier>();

  /* Generate and solve constraints for the non-recursive functions
   * in topological order for the call graph.
   */
  auto nonRecursiveFuncs = topoSortNonRecursive(cg);

  /* Auto-generalize: mark every non-recursive singleton-SCC function as
   * polymorphic so that call sites instantiate fresh type variables.
   */
  for (auto f : nonRecursiveFuncs)
    symbols->setPoly(f->getName());

  for (auto f : nonRecursiveFuncs) {
    SEMANTIC_LOG(2, "type-inference")
      << "function=" << f->getName() << " stage=polymorphic";

    PolyTypeConstraintCollectVisitor polyVisitor(symbols, cg, unifier);
    f->accept(&polyVisitor);

    unifier->add(polyVisitor.getCollectedConstraints());
    unifier->solve();
  }

  SEMANTIC_LOG(2, "type-inference") << "stage=residual-monomorphic start";

  /* Iterate over functions those that are recursive, or that may directly
   * or indirectly call a recursive function, generate their constraints.
   */
  for (auto f : cg->getVertices()) {
    // Skip the functions for which polymorphic inference was applied
    if (std::find(nonRecursiveFuncs.begin(), nonRecursiveFuncs.end(), f) ==
        nonRecursiveFuncs.end()) {
      TypeConstraintCollectVisitor monoVisitor(symbols);
      f->accept(&monoVisitor);
      unifier->add(monoVisitor.getCollectedConstraints());
    }
  }

  /* Solve monomorphic constraints in combination with the
   * previously collected polymorphic constraints.
   */
  unifier->solve();

  return std::make_shared<TypeInference>(symbols, unifier);
}

/*
 * Performs monomorphic type inference on the entire program.
 */
std::shared_ptr<TypeInference> runMono(ASTProgram *ast, SymbolTable *symbols) {
  SEMANTIC_LOG(2, "type-inference") << "stage=monomorphic start";

  TypeConstraintCollectVisitor visitor(symbols);
  ast->accept(&visitor);

  SEMANTIC_LOG(2, "type-inference") << "stage=solve start";

  auto unifier = std::make_shared<Unifier>(visitor.getCollectedConstraints());
  unifier->solve();

  return std::make_shared<TypeInference>(symbols, unifier);
}

/*
 * This implementation collects the constraints and then solves them with a
 * unifier instance.  The unifier then records the inferred type results that
 * can be subsequently queried.
 */
std::shared_ptr<TypeInference> TypeInference::run(ASTProgram *ast,
                                                  CallGraph *cg,
                                                  SymbolTable *symbols) {
  SEMANTIC_LOG(1, "type-inference") << "start";
  auto result = runPoly(ast, symbols, cg);
  SEMANTIC_LOG(1, "type-inference")
      << "complete functions=" << symbols->getFunctions().size();
  return result;
}

std::shared_ptr<TopType> TypeInference::getInferredType(ASTDeclNode *node) {
  auto var = std::make_shared<TopVar>(node);
  return unifier->inferred(var);
};

std::shared_ptr<TopType> TypeInference::getInferredType(ASTExpr *node) {
  auto var = std::make_shared<TopVar>(node);
  return unifier->inferred(var);
};

std::string TypeInference::getInferredTypeDisplay(ASTSumTypeDecl *node) {
  std::ostringstream s;
  auto firstVariant = true;
  for (auto *variant : node->getVariants()) {
    if (!firstVariant)
      s << " | ";
    firstVariant = false;
    s << variant->getTag();

    auto params = variant->getParams();
    if (params.empty())
      continue;

    s << "(";
    for (std::size_t i = 0; i < params.size(); ++i) {
      if (i > 0)
        s << ", ";
      auto payload = getInferredType(params[i]);
      if (auto *sum = dynamic_cast<TopSumType *>(payload.get())) {
        s << sum->getTypeName();
      } else if (auto *mu = dynamic_cast<TopMu *>(payload.get())) {
        if (auto *sum = dynamic_cast<TopSumType *>(mu->getT().get()))
          s << sum->getTypeName();
        else
          s << *payload;
      } else {
        s << *payload;
      }
    }
    s << ")";
  }
  return s.str();
}

void TypeInference::print(std::ostream &s) {
  auto typeNames = symbols->getSumTypes();
  if (!typeNames.empty()) {
    s << "\nTypes : {\n";
    const char *sep = "  ";
    for (const auto &name : typeNames) {
      auto *type = symbols->getSumType(name);
      s << sep << name << " : " << getInferredTypeDisplay(type);
      sep = ",\n  ";
    }
    s << "\n}\n";
  }

  s << "\nFunctions : {\n";
  const char *sep = "  ";
  for (auto f : symbols->getFunctions()) {
    s << sep << f->getName() << " : " << *getInferredType(f);
    sep = ",\n  ";
  }
  s << "\n}\n";

  for (auto f : symbols->getFunctions()) {
    s << "\nLocals for function " + f->getName() + " : {\n";
    sep = "  ";
    for (auto l : symbols->getLocals(f)) {
      s << sep << l->getName() << " : " << *getInferredType(l);
      sep = ",\n  ";
    }
    s << "\n}\n";
  }
}

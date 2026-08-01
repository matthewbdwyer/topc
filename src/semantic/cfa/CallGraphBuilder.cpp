#include "CallGraphBuilder.h"
#include "../SemanticLogging.h"

CallGraphBuilder CallGraphBuilder::build(ASTProgram *ast, CFAnalyzer cfa) {
  CallGraphBuilder cgb(cfa);
  ast->accept(&cgb);
  return cgb;
}

CallGraphBuilder::CallGraphBuilder(CFAnalyzer p) : cfa(p) {}

bool CallGraphBuilder::visit(ASTFunction *element) {
  cfun = element;
  return true;
}

bool CallGraphBuilder::visit(ASTFunAppExpr *element) {
  std::set<ASTFunction *> called;
  for (ASTFunction *f :
       cfa.getPossibleFunctionsForExpr(element->getFunction(), cfun)) {
    SEMANTIC_LOG(2, "call-graph")
      << "add caller=" << cfun->getName() << " callee=" << f->getName()
      << " call=" << *element;
    called.emplace(f);
    graph[cfun].insert(f);
    fromFunNameToASTFun[cfun->getName()] = cfun;
    fromFunNameToASTFun[f->getName()] = f;
  }
  mayCall.insert(
      std::pair<ASTFunAppExpr *, std::set<ASTFunction *>>(element, called));
  callSiteCaller[element] = cfun;
  return true;
} // LCOV_EXCL_LINE

std::map<ASTFunction *, std::set<ASTFunction *>>
CallGraphBuilder::getCallGraph() {
  return graph;
}

std::map<ASTFunAppExpr *, std::set<ASTFunction *>>
CallGraphBuilder::getMayCall() {
  return mayCall;
}

std::map<ASTFunAppExpr *, ASTFunction *> CallGraphBuilder::getCallSiteCaller() {
  return callSiteCaller;
}

std::map<std::string, ASTFunction *> CallGraphBuilder::getFunMap() {

  return fromFunNameToASTFun;
}

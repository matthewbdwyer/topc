#include "SymbolTable.h"
#include "FunctionNameCollector.h"
#include "LocalNameCollector.h"
#include "TypeNameCollector.h"
#include "../SemanticLogging.h"

#include <sstream>

std::shared_ptr<SymbolTable> SymbolTable::build(ASTProgram *p) {
  SEMANTIC_LOG(1, "symbol-table") << "start";
  auto fMap = FunctionNameCollector::build(p);
  auto lMap = LocalNameCollector::build(p, fMap);
  auto [tMap, cMap] = TypeNameCollector::build(p);
  std::size_t localCount = 0;
  for (const auto &scope : lMap) {
    localCount += scope.second.size();
  }
  SEMANTIC_LOG(1, "symbol-table")
      << "complete functions=" << fMap.size() << " locals=" << localCount
      << " types=" << tMap.size() << " constructors=" << cMap.size();
  return std::make_shared<SymbolTable>(fMap, lMap, tMap, cMap);
}

ASTDeclNode *SymbolTable::getFunction(std::string s) {
  auto func = functionNames.find(s);
  if (func == functionNames.end()) {
    return nullptr;
  }
  return func->second.first;
}

bool SymbolTable::getPoly(std::string s) {
  auto func = functionNames.find(s);
  if (func == functionNames.end()) {
    return false;
  }
  return func->second.second;
}

void SymbolTable::setPoly(std::string s) {
  auto func = functionNames.find(s);
  if (func != functionNames.end()) {
    func->second.second = true;
  }
}

std::vector<ASTDeclNode *> SymbolTable::getFunctions() {
  std::vector<ASTDeclNode *> funDecls;
  for (auto &pair : functionNames) {
    funDecls.push_back(pair.second.first);
  }
  return funDecls;
}

ASTDeclNode *SymbolTable::getLocal(std::string s, ASTDeclNode *f) {
  auto lMap = localNames.find(f)->second;
  auto local = lMap.find(s);
  if (local == lMap.end()) {
    return nullptr;
  }
  return local->second;
}

std::vector<ASTDeclNode *> SymbolTable::getLocals(ASTDeclNode *f) {
  auto lMap = localNames.find(f)->second;
  std::vector<ASTDeclNode *> localDecls;
  for (auto &pair : lMap) {
    localDecls.push_back(pair.second);
  }
  return localDecls;
}

ASTSumTypeDecl *SymbolTable::getSumType(std::string name) {
  auto it = typeNames.find(name);
  return it == typeNames.end() ? nullptr : it->second;
}

ASTSumVariant *SymbolTable::getConstructor(std::string tag) {
  auto it = constructorNames.find(tag);
  return it == constructorNames.end() ? nullptr : it->second;
}

std::vector<std::string> SymbolTable::getSumTypes() {
  std::vector<std::string> names;
  for (auto &p : typeNames)
    names.push_back(p.first);
  return names;
}

ASTSumTypeDecl *SymbolTable::getConstructorOwner(std::string tag) {
  for (auto &p : typeNames) {
    for (auto *v : p.second->getVariants()) {
      if (v->getTag() == tag)
        return p.second;
    }
  }
  return nullptr;
}

void SymbolTable::print(std::ostream &s) {
  s << "Functions : {";
  const char *sep = "";
  for (const auto &e : functionNames) {
    s << sep << e.first;
    sep = ", ";
  }
  s << "}\n";

  for (const auto &f : localNames) {
    s << "Locals for function " + f.first->getName() + " : {";
    sep = "";
    for (const auto &l : f.second) {
      s << sep << l.first;
      sep = ", ";
    }
    s << "}\n";
  }
}

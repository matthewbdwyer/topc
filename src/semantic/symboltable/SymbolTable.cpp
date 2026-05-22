#include "SymbolTable.h"
#include "FieldNameCollector.h"
#include "FunctionNameCollector.h"
#include "LocalNameCollector.h"
#include "TypeNameCollector.h"

#include <sstream>

#include "loguru.hpp"

std::shared_ptr<SymbolTable> SymbolTable::build(ASTProgram *p) {
  LOG_S(1) << "Building symbol table";
  auto fMap = FunctionNameCollector::build(p);
  auto lMap = LocalNameCollector::build(p, fMap);
  auto fSet = FieldNameCollector::build(p);
  auto [tMap, cMap] = TypeNameCollector::build(p);
  return std::make_shared<SymbolTable>(fMap, lMap, fSet, tMap, cMap);
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

std::vector<std::string> SymbolTable::getFields() { return fieldNames; }

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
  auto skip = true;
  for (auto e : functionNames) {
    if (skip) {
      skip = false;
      s << e.first;
      continue;
    }
    s << ", " + e.first;
  }
  s << "}\n";

  s << "Fields : {";
  skip = true;
  for (auto n : fieldNames) {
    if (skip) {
      skip = false;
      s << n;
      continue;
    }
    s << ", " + n;
  }
  s << "}\n";

  for (auto f : localNames) {
    s << "Locals for function " + f.first->getName() + " : {";
    skip = true;
    for (auto l : f.second) {
      if (skip) {
        skip = false;
        s << l.first;
        continue;
      }
      s << ", " + l.first;
    }
    s << "}\n";
  }
}

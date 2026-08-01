#pragma once

#include "ASTVisitor.h"
#include <map>
#include <string>

/*! \class TypeNameCollector
 *  \brief Collects sum type and constructor declarations from a program.
 *
 * Walks `ASTSumTypeDecl` nodes and records:
 *  - type name → `ASTSumTypeDecl*`
 *  - constructor tag → `ASTSumVariant*`
 *
 * Duplicate constructor names within the same type declaration are rejected
 * with a `SemanticError`.  Cross-type duplicates are already rejected by
 * `CheckSumTypeNames` before this pass runs.
 */
class TypeNameCollector : public ASTVisitor {
public:
  TypeNameCollector() = default;

  // Public maps so the static method can extract them.
  std::map<std::string, ASTSumTypeDecl *> typeMap;       // type name  → decl
  std::map<std::string, ASTSumVariant *> constructorMap; // ctor tag   → variant

  static std::pair<std::map<std::string, ASTSumTypeDecl *>,
                   std::map<std::string, ASTSumVariant *>>
  build(ASTProgram *p);

  virtual void endVisit(ASTSumTypeDecl *element) override;
};

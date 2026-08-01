#pragma once

#include "ASTDeclNode.h"
#include "SymbolTable.h"
#include "TypeInference.h"
#include "TopType.h"

#include <map>
#include <memory>

/*!
 * \enum OwnershipClass
 * \brief Ownership classification for a variable's type.
 *
 * - `Copy` — the variable's type is trivially copyable; no ownership tracking
 *             is required (int, function, borrow ref).
 * - `Own`  — the variable holds a uniquely-owned heap resource; ownership must
 *             be tracked through moves and must be freed on scope exit.
 */
enum class OwnershipClass { Copy, Own };

/*!
 * \class OwnershipClassifier
 * \brief Phase 8 pass: classify every declared variable as Copy or Own.
 *
 * Classification rules (from the dev plan):
 * | Type                     | Class                                    |
 * |--------------------------|------------------------------------------|
 * | TopInt                   | Copy                                     |
 * | TopFunction(...)         | Copy                                     |
 * | TopOwningRef(T)          | Own                                      |
 * | TopBorrowRef(T)          | Copy                                     |
 * | TopSumType(...)          | Own if any ctor payload is Own, else Copy|
 * | TopAlpha / TopVar        | Copy (unresolved; re-classified post-unif)|
 *
 * \sa OwnershipClass
 */
class OwnershipClassifier {
public:
  /*!
   * \brief Classify every ASTDeclNode reachable from the symbol table.
   *
   * Iterates over all function and local declarations, looks up the solved
   * type from \p typeInf, and stores the result in an internal map.
   *
   * \param symTable  The symbol table for the program.
   * \param typeInf   The completed type inference result.
   */
  OwnershipClassifier(SymbolTable *symTable, TypeInference *typeInf);

  /*!
   * \brief Return the ownership class for a given declaration node.
   *
   * \param node  An ASTDeclNode present in the program's symbol table.
   * \return The OwnershipClass assigned to \p node.
   */
  OwnershipClass classify(ASTDeclNode *node) const;

  /*!
   * \brief Classify a type term directly (does not consult the stored map).
   *
   * This is the structural recursive classification rule applied to an
   * arbitrary, fully-solved TopType.
   */
  static OwnershipClass classifyType(const TopType *type);

private:
  std::map<ASTDeclNode *, OwnershipClass> classes;
};

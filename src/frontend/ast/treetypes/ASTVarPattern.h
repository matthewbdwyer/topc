#pragma once

#include "ASTDeclNode.h"
#include "ASTPattern.h"

#include <memory>
#include <string>

/*! \brief Pattern that binds a value to a named variable.
 *
 * Example: the `x` in `Some(x)`.
 * Embeds an ASTDeclNode so that the symbol table infrastructure can
 * register the binding without pattern-specific changes in Phase B1.
 */
class ASTVarPattern : public ASTPattern {
  std::shared_ptr<ASTDeclNode> DECL;

public:
  explicit ASTVarPattern(std::shared_ptr<ASTDeclNode> decl)
      : DECL(std::move(decl)) {}

  const std::string &getName() const { return DECL->getName(); }
  ASTDeclNode *getDecl() const { return DECL.get(); }
  std::shared_ptr<ASTDeclNode> getDeclShared() const { return DECL; }

  std::ostream &print(std::ostream &out) const override {
    return out << DECL->getName();
  }
};

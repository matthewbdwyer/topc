#pragma once

#include "ASTDeclNode.h"
#include "ASTPattern.h"
#include "ASTStmt.h"

#include <string>
#include <vector>

/*! \brief Class for a single arm in a case statement.
 *
 * Example: `Some(v) -> output v;`
 *          `Some({r: x}) -> output x;`
 *          `Some(_) -> output 0;`
 *
 * PATTERNS holds the top-level payload patterns (one per payload position).
 * BINDINGS is a flat list of all ASTDeclNodes found in variable sub-patterns,
 * derived from PATTERNS.  It is preserved for backward compatibility with
 * visitors (LocalNameCollector, CodeGenVisitor) that were written before
 * nested patterns were introduced.
 */
class ASTCaseArm : public ASTNode {
  std::string TAG;
  std::vector<std::shared_ptr<ASTPattern>> PATTERNS;
  std::vector<std::shared_ptr<ASTDeclNode>> BINDINGS; // derived from PATTERNS

  // Recursively collect ASTDeclNodes from variable sub-patterns.
  static void collectBindings(ASTPattern *p,
                               std::vector<std::shared_ptr<ASTDeclNode>> &out);

  std::shared_ptr<ASTStmt> BODY;

public:
  std::vector<std::shared_ptr<ASTNode>> getChildren() override;
  ASTCaseArm(std::string tag,
             std::vector<std::shared_ptr<ASTPattern>> patterns,
             std::shared_ptr<ASTStmt> body);
  const std::string &getTag() const { return TAG; }
  std::vector<ASTPattern *> getPatterns() const;
  std::vector<ASTDeclNode *> getBindings() const; // flat var bindings
  ASTStmt *getBody() const { return BODY.get(); }
  void accept(ASTVisitor *visitor) override;

protected:
  std::ostream &print(std::ostream &out) const override;
};

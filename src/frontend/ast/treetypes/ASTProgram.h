#pragma once

#include "ASTFunction.h"
#include "ASTSumTypeDecl.h"
#include <memory>
#include <ostream>

class SemanticAnalysis;
namespace llvm { class Module; }

/*! \brief Class for a program: a list of type declarations and functions.
 *
 */
class ASTProgram : public ASTNode {
  std::string name;
  std::vector<std::shared_ptr<ASTSumTypeDecl>> TYPEDECLS;
  std::vector<std::shared_ptr<ASTFunction>> FUNCTIONS;

public:
  std::vector<std::shared_ptr<ASTNode>> getChildren() override;
  // Full constructor (TOP programs may have type declarations).
  ASTProgram(std::vector<std::shared_ptr<ASTSumTypeDecl>> typedecls,
             std::vector<std::shared_ptr<ASTFunction>> functions);
  // Backward-compat constructor for pure TOP programs without type decls.
  explicit ASTProgram(std::vector<std::shared_ptr<ASTFunction>> functions)
      : ASTProgram({}, std::move(functions)) {}
  void setName(std::string n) { name = n; }
  const std::string &getName() const { return name; }
  std::vector<ASTSumTypeDecl *> getTypedecls() const;
  std::vector<ASTFunction *> getFunctions() const;
  //! Append a synthesized top-level function (used by lambda desugaring).
  void addFunction(std::shared_ptr<ASTFunction> f) {
    FUNCTIONS.push_back(std::move(f));
  }
  ASTFunction *findFunctionByName(std::string);
  void accept(ASTVisitor *visitor) override;
  std::shared_ptr<llvm::Module> codegen(SemanticAnalysis *st, const std::string& name);

public:
  friend std::ostream &operator<<(std::ostream &os, const ASTProgram &obj) {
    return obj.print(os);
  }

protected:
  std::ostream &print(std::ostream &out) const override;
};

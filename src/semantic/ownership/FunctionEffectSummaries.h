#pragma once

#include "ASTDeclNode.h"
#include "ASTProgram.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

class ASTFunction;
class OwnershipClassifier;
class SymbolTable;
class TypeInference;

class FunctionEffectSummaries {
public:
  enum class FormalMode { Copy, Own, DependsOnInstantiation };

  enum class ReturnOrigin {
    Unknown,
    PureCopy,
    FreshOwn,
    FromFormal,
    BorrowFromFormal,
  };

  struct Summary {
    std::string functionName;
    std::vector<FormalMode> formalModes;
    ReturnOrigin returnOrigin = ReturnOrigin::Unknown;
    int returnFormalIndex = -1;
  };

  static std::shared_ptr<FunctionEffectSummaries>
  build(ASTProgram *ast, SymbolTable *sym, TypeInference *types,
        OwnershipClassifier *classifier);

  const Summary *get(ASTDeclNode *functionDecl) const;

private:
  std::map<ASTDeclNode *, Summary> summaries;
};

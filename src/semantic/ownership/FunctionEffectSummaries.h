#pragma once

#include "ASTDeclNode.h"
#include "ASTProgram.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

class ASTFunAppExpr;
class ASTFunction;
class CallGraph;
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

  /*! \brief What a generic body needs of an actual, decided per call site.
   *
   * A body that dereferences a borrowed formal in a position that takes the
   * value (`read(p) { return *p; }`) is sound only when the referent is Copy;
   * one that passes `&x` on to such a callee needs `x` itself to be Copy.
   * The type is a variable at the definition, so the check moves to the call.
   */
  enum class CopyRequirement { None, Referent, Self, Both };

  /*! \brief Why a requirement was recorded; selects the diagnostic. A
   *  bitmask, since one formal can need Copy for several reasons. */
  enum RequirementReason : unsigned {
    MovesOutOfBorrow = 1u << 0,  ///< `*p` taken where p is a borrowed formal
    OverwritesThroughBorrow = 1u << 1, ///< `*p = v` replaces the referent
    LendsToMoveOut = 1u << 2,    ///< `&x` handed to a callee that takes `*p`
    UsedAfterPassedOn = 1u << 3, ///< formal used again after passing it on
  };

  /*! \brief A requirement together with the reasons it was recorded. */
  struct FormalRequirement {
    CopyRequirement kind = CopyRequirement::None;
    unsigned reasons = 0;
  };

  struct Summary {
    std::string functionName;
    std::vector<std::string> formalNames;
    std::vector<FormalMode> formalModes;
    /*! For each formal: on every path through the body the value is passed on
     *  as an argument of some call (so a callee takes responsibility for it). */
    std::vector<bool> formalForwarded;
    /*! For each formal: what its actual must be for the body to be sound. */
    std::vector<CopyRequirement> formalRequirement;
    /*! For each formal: RequirementReason bits explaining formalRequirement. */
    std::vector<unsigned> formalRequirementReasons;
    ReturnOrigin returnOrigin = ReturnOrigin::Unknown;
    int returnFormalIndex = -1;
  };

  /*! \brief What one call site does to each of its actuals.
   *
   * consumes[i] is true when the callee takes ownership of actual i: the
   * caller must treat an Own variable passed there as moved. Computed once,
   * from solved types and the callee summaries (via the call graph for calls
   * through function values), so that MoveAnalysis and DestructionPass read
   * the same decision.
   */
  struct CallEffect {
    std::vector<bool> consumes;
  };

  /*! \param requirements Per-function requirements seeded by AliasCheck;
   *  may be null. Propagated to callers and checked at every call site. */
  static std::shared_ptr<FunctionEffectSummaries>
  build(ASTProgram *ast, SymbolTable *sym, TypeInference *types,
        OwnershipClassifier *classifier, CallGraph *cg,
        const std::map<ASTDeclNode *, std::vector<FormalRequirement>>
            *requirements = nullptr);

  const Summary *get(ASTDeclNode *functionDecl) const;

  /*! \brief Effect of a call site, or nullptr if the call was not analyzed. */
  const CallEffect *callEffect(const ASTFunAppExpr *call) const;

private:
  std::map<ASTDeclNode *, Summary> summaries;
  std::map<const ASTFunAppExpr *, CallEffect> callEffects;
};

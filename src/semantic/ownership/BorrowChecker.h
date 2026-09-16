#pragma once

#include "ASTVisitor.h"
#include "FunctionEffectSummaries.h"

#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <vector>

class ASTBinaryExpr;
class ASTBorrowExpr;
class ASTErrorStmt;
class ASTFunAppExpr;
class ASTFunction;
class ASTOutputStmt;
class ASTProgram;
class ASTReturnStmt;
class OwnershipClassifier;
class SymbolTable;
class TypeInference;

/*! \class BorrowChecker
 *  \brief Where a borrow, a borrow-derived value, or an alias may appear
 *         (kind: position).
 *
 * Every rule here is decided by the position of an expression in its parent:
 *
 * | value                                  | may appear                                   | elsewhere                       |
 * | -------------------------------------- | -------------------------------------------- | ------------------------------- |
 * | `&x`                                   | as a call argument                           | borrow-position                 |
 * | a borrow-derived call result           | as a call argument, under `*`                | borrow-escape (assignment, return, constructor payload) |
 * | an alias: an Own-typed `*e` on a borrow, or an Own-typed binder of `case *e` | under `&`, under `*`, as a `case *` scrutinee, as a write target `*e = v` (Copy slot) | alias-move-out, alias-binder, alias-overwrite |
 *
 * check() runs with the weeding passes, before types, and needs no types:
 * `&x` is a borrow syntactically. It names the position where it can (an
 * operand of arithmetic or comparison, the argument of `output` or `error`, a
 * `return`) and otherwise reports the general rule.
 *
 * checkPositions() runs after FunctionEffectSummaries::build, whose return
 * origins tell which call results are borrow-derived, and before
 * FunctionEffectSummaries::resolveRequirements: where a generic body takes
 * `*p` and the referent's type is still a variable, the decision belongs to
 * each call, so it returns a Copy requirement on the formal instead of
 * rejecting.
 *
 * Because every borrow is call-scoped, no region analysis is needed: a borrow
 * is dead when its callee returns.
 */
class BorrowChecker : public ASTVisitor {
public:
  struct BorrowTraceEvent {
    int line;
    int column;
    std::string expr;
    bool approved;
  };

  struct BorrowProvenanceEvent {
    enum class Kind { Direct, Flow };

    Kind kind;
    int originLine;
    int originColumn;
    std::string originExpr;
    int hop;
    int useLine;
    int useColumn;
    std::string expression;
    std::string callee;
    std::size_t argumentIndex;
  };

  using Requirements =
      std::map<ASTDeclNode *,
               std::vector<FunctionEffectSummaries::FormalRequirement>>;

  /*! \brief Before types: `&x` only as a call argument. */
  static void check(ASTProgram *p);

  /*! \brief After summaries: borrow-derived values and aliases. Returns the
   *  Copy requirements generic bodies impose on their callers. */
  static Requirements checkPositions(ASTProgram *p, SymbolTable *sym,
                                     TypeInference *types,
                                     OwnershipClassifier *classifier,
                                     FunctionEffectSummaries *effects);

  /*! \brief Returns retained borrow trace from the most recent run. */
  static const std::vector<BorrowTraceEvent> &getLastTrace();

  /*! \brief Returns retained provenance from the most recent run. */
  static const std::vector<BorrowProvenanceEvent> &getLastProvenance();

private:
  class PositionWalk;

  BorrowChecker() = default;

  // Set of ASTBorrowExpr nodes that appear as direct arguments of a call.
  std::set<ASTBorrowExpr *> approvedBorrows;
  std::vector<BorrowTraceEvent> trace;
  std::vector<BorrowProvenanceEvent> provenance;
  static std::vector<BorrowTraceEvent> lastTrace;
  static std::vector<BorrowProvenanceEvent> lastProvenance;

  bool visit(ASTFunAppExpr *element) override;
  bool visit(ASTBinaryExpr *element) override;
  bool visit(ASTOutputStmt *element) override;
  bool visit(ASTErrorStmt *element) override;
  bool visit(ASTReturnStmt *element) override;
  void endVisit(ASTBorrowExpr *element) override;

  static void addProvenance(std::vector<BorrowProvenanceEvent> &events,
                            BorrowProvenanceEvent event);
  static void publish(std::vector<BorrowTraceEvent> trace,
                      std::vector<BorrowProvenanceEvent> provenance);
};

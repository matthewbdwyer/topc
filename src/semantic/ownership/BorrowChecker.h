#pragma once

#include "ASTVisitor.h"

#include <cstddef>
#include <set>
#include <string>
#include <vector>

class ASTAssignStmt;
class ASTBinaryExpr;
class ASTProgram;
class ASTBorrowExpr;
class ASTErrorStmt;
class ASTExpr;
class ASTFunAppExpr;
class ASTFunction;
class ASTIfStmt;
class ASTOutputStmt;
class ASTReturnStmt;
class ASTWhileStmt;
class FunctionEffectSummaries;
class SymbolTable;

/*! \class BorrowChecker
 *  \brief Phase 10 — Borrow/Lifetime Validity.
 *
 * Enforces the immediate-argument restriction (Q21): a borrow expression
 * (`&x`) is legal **only** as a direct argument of a function call.
 * Storing a borrow in a variable, using it in a condition, or in any
 * other position is a SemanticError.
 *
 * Because borrows are proven by this pass to be call-scoped, no CFG or
 * lifetime region analysis is required: the borrow is dead as soon as the
 * callee returns, so the owner may safely be moved afterwards.
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

  static void check(ASTProgram *p);

  static void checkInterprocedural(ASTProgram *p, SymbolTable *sym,
                                   FunctionEffectSummaries *effects);

  /*! \brief Returns retained borrow trace from the most recent run. */
  static const std::vector<BorrowTraceEvent> &getLastTrace();

  /*! \brief Returns retained provenance from the most recent run. */
  static const std::vector<BorrowProvenanceEvent> &getLastProvenance();

private:
  struct BorrowOrigin {
    bool derived = false;
    bool hasConcreteOrigin = false;
    int line = 0;
    int column = 0;
    std::string expr;
    int hop = 0;
  };

  BorrowChecker() = default;
  BorrowChecker(SymbolTable *sym, FunctionEffectSummaries *effects)
      : sym(sym), functionEffects(effects), checkCallReturns(true) {}

  // Set of ASTBorrowExpr nodes that appear as direct arguments of a call.
  std::set<ASTBorrowExpr *> approvedBorrows;
  std::vector<BorrowTraceEvent> trace;
  std::vector<BorrowProvenanceEvent> provenance;
  static std::vector<BorrowTraceEvent> lastTrace;
  static std::vector<BorrowProvenanceEvent> lastProvenance;
  SymbolTable *sym = nullptr;
  FunctionEffectSummaries *functionEffects = nullptr;
  ASTDeclNode *currentFuncDecl = nullptr;
  bool checkCallReturns = false;

  bool visit(ASTFunction *element) override;
  bool visit(ASTFunAppExpr *element) override;
  void endVisit(ASTBorrowExpr *element) override;
  void endVisit(ASTAssignStmt *element) override;
  void endVisit(ASTBinaryExpr *element) override;
  void endVisit(ASTOutputStmt *element) override;
  void endVisit(ASTErrorStmt *element) override;
  void endVisit(ASTReturnStmt *element) override;
  void endVisit(ASTIfStmt *element) override;
  void endVisit(ASTWhileStmt *element) override;

  BorrowOrigin borrowOrigin(ASTExpr *expr) const;
  bool exprReturnsBorrow(ASTExpr *expr) const;
  void addProvenance(BorrowProvenanceEvent event);
  void publishResults();
  void rejectBorrowEscape(ASTExpr *expr, const std::string &sink) const;
};

#pragma once

#include "CodeGenContext.h"
#include "llvm/IR/Value.h"

#include <memory>
#include <string>

// Forward-declare all AST node types to avoid pulling in AST headers.
class ASTAllocExpr;
class ASTAssignStmt;
class ASTBinaryExpr;
class ASTBlockStmt;
class ASTCaseArm;
class ASTCaseStmt;
class ASTDeclNode;
class ASTDeclStmt;
class ASTDeRefExpr;
 class ASTDestroyStmt;
class ASTErrorStmt;
class ASTFunAppExpr;
class ASTFunction;
class ASTIfStmt;
class ASTInputExpr;
class ASTNode;
class ASTNumberExpr;
class ASTOutputStmt;
class ASTPattern;
class ASTProgram;
class ASTBorrowExpr;
class ASTReturnStmt;
class ASTSumCtorExpr;
class ASTVariableExpr;
class ASTWhileStmt;
class SemanticAnalysis;
class TopType;

/*! \class CodeGenVisitor
 *  \brief Walks an AST and emits LLVM IR into a CodeGenContext.
 *
 *  All LLVM-specific code generation logic lives here.  The AST node
 *  hierarchy is kept clean of LLVM dependencies; this visitor provides the
 *  only bridge between the two.
 */
class CodeGenVisitor {
public:
  CodeGenVisitor() = default;

  /*! \brief Set the context for use during unit testing.
   *
   *  In normal operation the context is created by the top-level generate().
   *  Unit tests that call dispatch() directly need to supply a context first.
   */
  void setContext(CodeGenContext *ctx) { ctx_ = ctx; }

  /*! \brief Top-level entry point: compile an entire TOP program.
   *
   *  Initialises the CodeGenContext, visits every function, verifies the
   *  module, and returns it.
   */
  std::shared_ptr<llvm::Module> generate(ASTProgram *program,
                                         SemanticAnalysis *semanticAnalysis,
                                         const std::string &programName);

  /*! \brief Dispatch to the correct generate() overload for any AST node. */
  llvm::Value *dispatch(ASTNode *node);

  // -------------------------------------------------------------------------
  // Per-node generate() overloads — one per concrete AST node type.
  // -------------------------------------------------------------------------
  llvm::Value *generate(ASTFunction     *node);
  llvm::Value *generate(ASTNumberExpr   *node);
  llvm::Value *generate(ASTBinaryExpr   *node);
  llvm::Value *generate(ASTVariableExpr *node);
  llvm::Value *generate(ASTInputExpr    *node);
  llvm::Value *generate(ASTFunAppExpr   *node);
  llvm::Value *generate(ASTAllocExpr    *node);
  llvm::Value *generate(ASTBorrowExpr     *node);
  llvm::Value *generate(ASTDeRefExpr    *node);

  llvm::Value *generate(ASTDeclNode     *node);
  llvm::Value *generate(ASTDeclStmt     *node);
  llvm::Value *generate(ASTAssignStmt   *node);
  llvm::Value *generate(ASTBlockStmt    *node);
  llvm::Value *generate(ASTWhileStmt    *node);
  llvm::Value *generate(ASTIfStmt       *node);
  llvm::Value *generate(ASTOutputStmt   *node);
  llvm::Value *generate(ASTErrorStmt    *node);
  llvm::Value *generate(ASTReturnStmt   *node);
  llvm::Value *generate(ASTDestroyStmt  *node);
  llvm::Value *generate(ASTSumCtorExpr  *node);
  llvm::Value *generate(ASTCaseStmt     *node);

private:
  // The context is created fresh for each top-level generate() call.
  CodeGenContext *ctx_ = nullptr;
  // Set during the top-level generate(); used by generate(ASTDestroyStmt*).
  SemanticAnalysis *semanticAnalysis_ = nullptr;

  /*! \brief Lazily declare free() in the module if not yet declared. */
  void ensureFreeDecl(CodeGenContext &ctx);

  /*! \brief Recursively emit free() calls for an owned value.
   *
   * \p ptrAsInt  An i64 holding the owning pointer value.
   * \p topType   The TopType of the owned value (expected to be TopOwningRef).
   */
  void emitDestroyValue(llvm::Value *ptrAsInt, TopType *topType,
                        CodeGenContext &ctx);

  /*! \brief Emit code to match a single pattern position within an arm.
   *
   * \p basePtr    Opaque pointer to the containing sum-type or record struct.
   * \p offset     GEP offset within basePtr (1 for first payload slot, etc.).
   * \p pat        The pattern to match.
   * \p paramDecl  The ASTDeclNode whose inferred type gives the slot's type
   *               (used for wildcard destruction on Own payloads; may be null).
   * \p failBB     Where to jump if the pattern test fails (nested-ctor only).
   * \p func       The containing LLVM function (for alloca / BB insertion).
   */
  void emitPatternMatch(llvm::Value *basePtr, int64_t offset, ASTPattern *pat,
                        ASTDeclNode *paramDecl,
                        llvm::BasicBlock *failBB,
                        llvm::Function *func,
                        CodeGenContext &ctx);
};

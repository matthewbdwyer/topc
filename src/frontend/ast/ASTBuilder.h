#pragma once

#include "AST.h"

#include "TOPBaseVisitor.h"
#include "TOPParser.h"
#include "antlr4-runtime.h"

#include <string>

using namespace antlrcpp;

/*! \brief Parse tree visitor that generates a program AST.
 *
 * This is an ANTLR4 parse tree visitor, not to be confused with an ASTVisitor.
 * As such its structure follows that of the ANTLR4 generated TOPBaseVisitor.
 * The primary entry point is the build method which initiates the traversal
 * of the parse tree and, if succesful, generates a shared ASTProgram whose
 * ownership is transferred to the caller.
 */
class ASTBuilder : public TOPBaseVisitor {
private:
  TOPParser *parser;
  std::string opString(int op);
  std::string generateSHA256(std::string tohash);

public:
  ASTBuilder(TOPParser *parser);

  /*! \fn build
   *  \brief Builds an instance of ASTProgram from an ANTLR4 parse tree.
   *
   * The caller obtains "ownership" of the resulting ASTProgram.
   */
  std::shared_ptr<ASTProgram> build(TOPParser::ProgramContext *ctx);

  /**
   * a helper function to build binary expressions
   */
  template <typename T> void visitBinaryExpr(T *ctx, const std::string &op);

  Any visitFunction(TOPParser::FunctionContext *ctx) override;
  Any visitNegNumber(TOPParser::NegNumberContext *ctx) override;
  Any visitAdditiveExpr(TOPParser::AdditiveExprContext *ctx) override;
  Any visitRelationalExpr(TOPParser::RelationalExprContext *ctx) override;
  Any visitMultiplicativeExpr(
      TOPParser::MultiplicativeExprContext *ctx) override;
  Any visitEqualityExpr(TOPParser::EqualityExprContext *ctx) override;
  Any visitParenExpr(TOPParser::ParenExprContext *ctx) override;
  Any visitNumExpr(TOPParser::NumExprContext *ctx) override;
  Any visitVarExpr(TOPParser::VarExprContext *ctx) override;
  Any visitInputExpr(TOPParser::InputExprContext *ctx) override;
  Any visitFunAppExpr(TOPParser::FunAppExprContext *ctx) override;
  Any visitAllocExpr(TOPParser::AllocExprContext *ctx) override;
  Any visitRefExpr(TOPParser::RefExprContext *ctx) override;
  Any visitDeRefExpr(TOPParser::DeRefExprContext *ctx) override;
  Any visitNullExpr(TOPParser::NullExprContext *ctx) override;
  Any visitDeclaration(TOPParser::DeclarationContext *ctx) override;
  Any visitNameDeclaration(TOPParser::NameDeclarationContext *ctx) override;
  Any visitAssignStmt(TOPParser::AssignStmtContext *ctx) override;
  Any visitBlockStmt(TOPParser::BlockStmtContext *ctx) override;
  Any visitWhileStmt(TOPParser::WhileStmtContext *ctx) override;
  Any visitIfStmt(TOPParser::IfStmtContext *ctx) override;
  Any visitOutputStmt(TOPParser::OutputStmtContext *ctx) override;
  Any visitErrorStmt(TOPParser::ErrorStmtContext *ctx) override;
  Any visitReturnStmt(TOPParser::ReturnStmtContext *ctx) override;

  // TOP extensions (Phase 1 stubs — AST nodes built in Phase 2)
  Any visitTypeDecl(TOPParser::TypeDeclContext *ctx) override;
  Any visitSumVariant(TOPParser::SumVariantContext *ctx) override;
  Any visitCaseStmt(TOPParser::CaseStmtContext *ctx) override;
  Any visitCaseArm(TOPParser::CaseArmContext *ctx) override;
  Any visitSumCtorExprWithArgs(TOPParser::SumCtorExprWithArgsContext *ctx) override;
  Any visitSumCtorExprNoArgs(TOPParser::SumCtorExprNoArgsContext *ctx) override;
};

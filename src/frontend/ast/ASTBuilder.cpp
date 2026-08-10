#include "ASTBuilder.h"

#include "ASTCtorPattern.h"
#include "ASTVarPattern.h"
#include "ASTWildcardPattern.h"
#include "InternalError.h"
#include "picosha2.h"

#include "loguru.hpp"
#include <functional>
#include <vector>

using namespace antlrcpp;

ASTBuilder::ASTBuilder(TOPParser *p) : parser{p} {}

std::string ASTBuilder::opString(int op) {
  std::string opStr;
  switch (op) {
  case TOPParser::MUL:
    opStr = "*";
    break;
  case TOPParser::DIV:
    opStr = "/";
    break;
  case TOPParser::ADD:
    opStr = "+";
    break;
  case TOPParser::SUB:
    opStr = "-";
    break;
  case TOPParser::GT:
    opStr = ">";
    break;
  case TOPParser::EQ:
    opStr = "==";
    break;
  case TOPParser::NE:
    opStr = "!=";
    break;
  default:
    throw std::runtime_error(
        "unknown operator :" +
        std::string(ASTBuilder::parser->getVocabulary().getLiteralName(op)));
  }
  return opStr;
}

/*
 * Globals for communicating information up from visited subtrees
 * These are overwritten by every visit call.
 * We use multiple variables here to avoid downcasting of shared smart pointers.
 */
static std::shared_ptr<ASTStmt> visitedStmt = nullptr;
static std::shared_ptr<ASTDeclNode> visitedDeclNode = nullptr;
static std::shared_ptr<ASTDeclStmt> visitedDeclStmt = nullptr;
static std::shared_ptr<ASTExpr> visitedExpr = nullptr;
static std::shared_ptr<ASTFunction> visitedFunction = nullptr;
static std::shared_ptr<ASTSumTypeDecl> visitedTypedecl = nullptr;
static std::shared_ptr<ASTSumVariant> visitedSumVariant = nullptr;
static std::shared_ptr<ASTCaseArm> visitedCaseArm = nullptr;
static std::shared_ptr<ASTPattern> visitedPattern = nullptr;

/**********************************************************************
 * These methods override selected methods in the TOPBaseVisitor.
 *
 * For each rule name in an ANTLR4 grammar a visit method and a
 * "context" class is generated.  The visit methods are defined in
 * TOPBaseVisitor.h and the context classes in TOPParser.h
 *
 * Only the methods for grammar rules that "directly" contain
 * content that must be processed need to be overridden.  For example,
 * for strings that match "program" you need to process the list of
 * functions.
 *
 * If you are new to visitors, it is very important to understand
 * that since you are selectively overriding methods you cannot
 * rely on the call stack to return values (since the values will
 * be lost by the methods you don't override).  Instead you must create
 * your own structure that is local to the visitor to communicate between
 * the calls during the visit.  In the case of this visitor it is the
 * visitedX variables above.
 *
 * Note that the visit methods are required to return a value, but
 * we make no use of that value, so we simply return the empty string ("")
 * since we cannot return a nullptr (leads to a crash in the visitor).
 *
 * Consult context class definitions to understand the type definitions
 * of the fields that contain the program elements captured during the parse.
 * You will access these from the method overrides in your visitor.
 */

std::shared_ptr<ASTProgram> ASTBuilder::build(TOPParser::ProgramContext *ctx) {
  std::vector<std::shared_ptr<ASTSumTypeDecl>> pTypedecls;
  for (auto td : ctx->typeDecl()) {
    visit(td);
    pTypedecls.push_back(visitedTypedecl);
  }

  std::vector<std::shared_ptr<ASTFunction>> pFunctions;
  for (auto fn : ctx->function()) {
    visit(fn);
    pFunctions.push_back(visitedFunction);
  }

  auto prog = std::make_shared<ASTProgram>(pTypedecls, pFunctions);
  prog->setName(generateSHA256(ctx->getText()));
  return prog;
}

Any ASTBuilder::visitFunction(TOPParser::FunctionContext *ctx) {
  std::shared_ptr<ASTDeclNode> fName;
  std::vector<std::shared_ptr<ASTDeclNode>> fParams;
  std::vector<std::shared_ptr<ASTDeclStmt>> fDecls;
  std::vector<std::shared_ptr<ASTStmt>> fBody;

  bool firstId = true;
  for (auto decl : ctx->nameDeclaration()) {
    visit(decl);
    if (firstId) {
      firstId = !firstId;
      fName = visitedDeclNode;
    } else {
      fParams.push_back(visitedDeclNode);
    }
  }

  for (auto decl : ctx->declaration()) {
    visit(decl);
    fDecls.push_back(visitedDeclStmt);
  }

  for (auto stmt : ctx->statement()) {
    visit(stmt);
    fBody.push_back(visitedStmt);
  }

  // return statement is always the last statement in a TOP function body
  visit(ctx->returnStmt());
  fBody.push_back(visitedStmt);

  visitedFunction =
      std::make_shared<ASTFunction>(fName, fParams, fDecls, fBody, false);

  LOG_S(1) << "Built AST node for function " << *visitedFunction;

  // Set source location
  visitedFunction->setLocation(ctx->getStart()->getLine(),
                               ctx->getStart()->getCharPositionInLine());
  return "";
}

Any ASTBuilder::visitNegNumber(TOPParser::NegNumberContext *ctx) {
  int val = std::stoi(ctx->NUMBER()->getText());
  val = -val;
  visitedExpr = std::make_shared<ASTNumberExpr>(val);

  LOG_S(1) << "Built AST node " << *visitedExpr;

  // Set source location
  visitedExpr->setLocation(ctx->getStart()->getLine(),
                           ctx->getStart()->getCharPositionInLine());
  return "";
} // LCOV_EXCL_LINE

/*
 * Unfortunately, the context types for binary expressions generated
 * by ANTLR are not organized into a sub-type hierarchy.  If they were
 * we could have a single method to construct binary expression nodes,
 * but as it stands we have some repetitive code to handle the different
 * context types.
 *
 * This might be improved by restructuring the grammar, but then another
 * mechanism for handling operator precedence would be needed.
 */
template <typename T>
void ASTBuilder::visitBinaryExpr(T *ctx, const std::string &op) {
  visit(ctx->expr(0));
  auto lhs = visitedExpr;

  visit(ctx->expr(1));
  auto rhs = visitedExpr;

  visitedExpr = std::make_shared<ASTBinaryExpr>(op, lhs, rhs);

  LOG_S(1) << "Built AST node " << *visitedExpr;

  // Set source location
  visitedExpr->setLocation(ctx->getStart()->getLine(),
                           ctx->getStart()->getCharPositionInLine());
}

Any ASTBuilder::visitAdditiveExpr(TOPParser::AdditiveExprContext *ctx) {
  visitBinaryExpr(ctx, opString(ctx->op->getType()));
  return "";
} // LCOV_EXCL_LINE

Any ASTBuilder::visitRelationalExpr(TOPParser::RelationalExprContext *ctx) {
  visitBinaryExpr(ctx, opString(ctx->op->getType()));
  return "";
} // LCOV_EXCL_LINE

Any ASTBuilder::visitMultiplicativeExpr(
    TOPParser::MultiplicativeExprContext *ctx) {
  visitBinaryExpr(ctx, opString(ctx->op->getType()));
  return "";
} // LCOV_EXCL_LINE

Any ASTBuilder::visitEqualityExpr(TOPParser::EqualityExprContext *ctx) {
  visitBinaryExpr(ctx, opString(ctx->op->getType()));
  return "";
} // LCOV_EXCL_LINE

Any ASTBuilder::visitParenExpr(TOPParser::ParenExprContext *ctx) {
  visit(ctx->expr());
  // leave visitedExpr from expr unchanged
  return "";
} // LCOV_EXCL_LINE

Any ASTBuilder::visitNumExpr(TOPParser::NumExprContext *ctx) {
  int val = std::stoi(ctx->NUMBER()->getText());
  visitedExpr = std::make_shared<ASTNumberExpr>(val);

  LOG_S(1) << "Built AST node " << *visitedExpr;

  // Set source location
  visitedExpr->setLocation(ctx->getStart()->getLine(),
                           ctx->getStart()->getCharPositionInLine());
  return "";
} // LCOV_EXCL_LINE

Any ASTBuilder::visitVarExpr(TOPParser::VarExprContext *ctx) {
  std::string name = ctx->IDENTIFIER()->getText();
  visitedExpr = std::make_shared<ASTVariableExpr>(name);

  LOG_S(1) << "Built AST node " << *visitedExpr;

  // Set source location
  visitedExpr->setLocation(ctx->getStart()->getLine(),
                           ctx->getStart()->getCharPositionInLine());
  return "";
}

Any ASTBuilder::visitInputExpr(TOPParser::InputExprContext *ctx) {
  visitedExpr = std::make_shared<ASTInputExpr>();

  LOG_S(1) << "Built AST node " << *visitedExpr;

  // Set source location
  visitedExpr->setLocation(ctx->getStart()->getLine(),
                           ctx->getStart()->getCharPositionInLine());
  return "";
} // LCOV_EXCL_LINE

Any ASTBuilder::visitFunAppExpr(TOPParser::FunAppExprContext *ctx) {
  std::shared_ptr<ASTExpr> fExpr = nullptr;
  std::vector<std::shared_ptr<ASTExpr>> fArgs;

  // First expression is the function, the rest are the args
  bool first = true;
  for (auto e : ctx->expr()) {
    visit(e);
    if (first) {
      fExpr = visitedExpr;
      first = false;
    } else {
      fArgs.push_back(visitedExpr);
    }
  }

  visitedExpr = std::make_shared<ASTFunAppExpr>(fExpr, fArgs);

  LOG_S(1) << "Built AST node " << *visitedExpr;

  // Set source location
  visitedExpr->setLocation(ctx->getStart()->getLine(),
                           ctx->getStart()->getCharPositionInLine());
  return "";
}

Any ASTBuilder::visitAllocExpr(TOPParser::AllocExprContext *ctx) {
  visit(ctx->expr());
  visitedExpr = std::make_shared<ASTAllocExpr>(visitedExpr);

  LOG_S(1) << "Built AST node " << *visitedExpr;

  // Set source location
  visitedExpr->setLocation(ctx->getStart()->getLine(),
                           ctx->getStart()->getCharPositionInLine());
  return "";
} // LCOV_EXCL_LINE

Any ASTBuilder::visitRefExpr(TOPParser::RefExprContext *ctx) {
  visit(ctx->expr());
  visitedExpr = std::make_shared<ASTBorrowExpr>(visitedExpr);

  LOG_S(1) << "Built AST node " << *visitedExpr;

  // Set source location
  visitedExpr->setLocation(ctx->getStart()->getLine(),
                           ctx->getStart()->getCharPositionInLine());
  return "";
} // LCOV_EXCL_LINE

Any ASTBuilder::visitDeRefExpr(TOPParser::DeRefExprContext *ctx) {
  visit(ctx->expr());
  visitedExpr = std::make_shared<ASTDeRefExpr>(visitedExpr);

  LOG_S(1) << "Built AST node " << *visitedExpr;

  // Set source location
  visitedExpr->setLocation(ctx->getStart()->getLine(),
                           ctx->getStart()->getCharPositionInLine());
  return "";
} // LCOV_EXCL_LINE

Any ASTBuilder::visitDeclaration(TOPParser::DeclarationContext *ctx) {
  std::vector<std::shared_ptr<ASTDeclNode>> dVars;
  for (auto decl : ctx->nameDeclaration()) {
    visit(decl);
    dVars.push_back(visitedDeclNode);
  }
  visitedDeclStmt = std::make_shared<ASTDeclStmt>(dVars);

  LOG_S(1) << "Built AST node " << *visitedDeclStmt;

  // Set source location
  visitedDeclStmt->setLocation(ctx->getStart()->getLine(),
                               ctx->getStart()->getCharPositionInLine());
  return "";
}

Any ASTBuilder::visitNameDeclaration(TOPParser::NameDeclarationContext *ctx) {
  std::string name = ctx->IDENTIFIER()->getText();
  visitedDeclNode = std::make_shared<ASTDeclNode>(name);

  LOG_S(1) << "Built AST node " << *visitedDeclNode;

  // Set source location
  visitedDeclNode->setLocation(ctx->getStart()->getLine(),
                               ctx->getStart()->getCharPositionInLine());
  return "";
}

Any ASTBuilder::visitBlockStmt(TOPParser::BlockStmtContext *ctx) {
  std::vector<std::shared_ptr<ASTStmt>> bStmts;
  for (auto s : ctx->statement()) {
    visit(s);
    bStmts.push_back(visitedStmt);
  }
  visitedStmt = std::make_shared<ASTBlockStmt>(bStmts);

  LOG_S(1) << "Built AST node " << *visitedStmt;

  // Set source location
  visitedStmt->setLocation(ctx->getStart()->getLine(),
                           ctx->getStart()->getCharPositionInLine());
  return "";
}

Any ASTBuilder::visitWhileStmt(TOPParser::WhileStmtContext *ctx) {
  visit(ctx->expr());
  auto cond = visitedExpr;
  visit(ctx->statement());
  auto body = visitedStmt;
  visitedStmt = std::make_shared<ASTWhileStmt>(cond, body);

  LOG_S(1) << "Built AST node " << *visitedStmt;

  // Set source location
  visitedStmt->setLocation(ctx->getStart()->getLine(),
                           ctx->getStart()->getCharPositionInLine());
  return "";
}

Any ASTBuilder::visitIfStmt(TOPParser::IfStmtContext *ctx) {
  visit(ctx->expr());
  auto cond = visitedExpr;
  visit(ctx->statement(0));
  auto thenBody = visitedStmt;

  // else is optional
  std::shared_ptr<ASTStmt> elseBody = nullptr;
  if (ctx->statement().size() == 2) {
    visit(ctx->statement(1));
    elseBody = visitedStmt;
  }

  visitedStmt = std::make_shared<ASTIfStmt>(cond, thenBody, elseBody);

  LOG_S(1) << "Built AST node " << *visitedStmt;

  // Set source location
  visitedStmt->setLocation(ctx->getStart()->getLine(),
                           ctx->getStart()->getCharPositionInLine());
  return "";
}

Any ASTBuilder::visitOutputStmt(TOPParser::OutputStmtContext *ctx) {
  visit(ctx->expr());
  visitedStmt = std::make_shared<ASTOutputStmt>(visitedExpr);

  LOG_S(1) << "Built AST node " << *visitedStmt;

  // Set source location
  visitedStmt->setLocation(ctx->getStart()->getLine(),
                           ctx->getStart()->getCharPositionInLine());
  return "";
} // LCOV_EXCL_LINE

Any ASTBuilder::visitErrorStmt(TOPParser::ErrorStmtContext *ctx) {
  visit(ctx->expr());
  visitedStmt = std::make_shared<ASTErrorStmt>(visitedExpr);

  LOG_S(1) << "Built AST node " << *visitedStmt;

  // Set source location
  visitedStmt->setLocation(ctx->getStart()->getLine(),
                           ctx->getStart()->getCharPositionInLine());
  return "";
} // LCOV_EXCL_LINE

Any ASTBuilder::visitReturnStmt(TOPParser::ReturnStmtContext *ctx) {
  visit(ctx->expr());
  visitedStmt = std::make_shared<ASTReturnStmt>(visitedExpr);

  LOG_S(1) << "Built AST node " << *visitedStmt;

  // Set source location
  visitedStmt->setLocation(ctx->getStart()->getLine(),
                           ctx->getStart()->getCharPositionInLine());
  return "";
} // LCOV_EXCL_LINE

// TOP extensions relative to TIP

Any ASTBuilder::visitSumVariant(TOPParser::SumVariantContext *ctx) {
  std::string tag = ctx->CONID()->getText();
  std::vector<std::shared_ptr<ASTDeclNode>> params;
  for (auto nd : ctx->nameDeclaration()) {
    visit(nd);
    params.push_back(visitedDeclNode);
  }
  visitedSumVariant = std::make_shared<ASTSumVariant>(tag, params);
  LOG_S(1) << "Built AST node " << *visitedSumVariant;
  visitedSumVariant->setLocation(ctx->getStart()->getLine(),
                                 ctx->getStart()->getCharPositionInLine());
  return "";
}

Any ASTBuilder::visitTypeDecl(TOPParser::TypeDeclContext *ctx) {
  std::string name = ctx->CONID()->getText();
  std::vector<std::shared_ptr<ASTSumVariant>> variants;
  for (auto sv : ctx->sumVariant()) {
    visit(sv);
    variants.push_back(visitedSumVariant);
  }
  visitedTypedecl = std::make_shared<ASTSumTypeDecl>(name, variants);
  LOG_S(1) << "Built AST node " << *visitedTypedecl;
  visitedTypedecl->setLocation(ctx->getStart()->getLine(),
                               ctx->getStart()->getCharPositionInLine());
  return "";
}

Any ASTBuilder::visitSumCtorExprWithArgs(TOPParser::SumCtorExprWithArgsContext *ctx) {
  std::string tag = ctx->CONID()->getText();
  std::vector<std::shared_ptr<ASTExpr>> args;
  for (auto *argCtx : ctx->expr()) {
    visit(argCtx);
    args.push_back(visitedExpr);
  }
  visitedExpr = std::make_shared<ASTSumCtorExpr>(tag, std::move(args));

  LOG_S(1) << "Built AST node " << *visitedExpr;

  visitedExpr->setLocation(ctx->getStart()->getLine(),
                           ctx->getStart()->getCharPositionInLine());
  return "";
} // LCOV_EXCL_LINE

Any ASTBuilder::visitSumCtorExprNoArgs(TOPParser::SumCtorExprNoArgsContext *ctx) {
  std::string tag = ctx->CONID()->getText();
  visitedExpr = std::make_shared<ASTSumCtorExpr>(tag, std::vector<std::shared_ptr<ASTExpr>>{});

  LOG_S(1) << "Built AST node " << *visitedExpr;

  visitedExpr->setLocation(ctx->getStart()->getLine(),
                           ctx->getStart()->getCharPositionInLine());
  return "";
} // LCOV_EXCL_LINE

Any ASTBuilder::visitPattern(TOPParser::PatternContext *ctx) {
  if (ctx->KWILDCARD()) {
    // Wildcard pattern: `_`
    visitedPattern = std::make_shared<ASTWildcardPattern>();
  } else if (ctx->CONID()) {
    // Constructor pattern: `None` (no args) or `Inner(x)` (with args)
    std::vector<std::shared_ptr<ASTPattern>> subs;
    for (auto sub : ctx->pattern()) {
      visit(sub);
      subs.push_back(visitedPattern);
    }
    visitedPattern = std::make_shared<ASTCtorPattern>(
        ctx->CONID()->getText(), std::move(subs));
  } else {
    // Variable pattern: single IDENTIFIER
    auto id = ctx->IDENTIFIER();
    auto decl = std::make_shared<ASTDeclNode>(id->getText());
    decl->setLocation(static_cast<int>(id->getSymbol()->getLine()),
                      static_cast<int>(id->getSymbol()->getCharPositionInLine()));
    visitedPattern = std::make_shared<ASTVarPattern>(decl);
  }
  return "";
}

Any ASTBuilder::visitCaseArm(TOPParser::CaseArmContext *ctx) {
  std::string tag = ctx->CONID()->getText();
  std::vector<std::shared_ptr<ASTPattern>> patterns;
  for (auto patCtx : ctx->pattern()) {
    visit(patCtx);
    patterns.push_back(visitedPattern);
  }
  visit(ctx->statement());
  auto body = visitedStmt;
  visitedCaseArm = std::make_shared<ASTCaseArm>(tag, patterns, body);
  LOG_S(1) << "Built AST node " << *visitedCaseArm;
  visitedCaseArm->setLocation(ctx->getStart()->getLine(),
                              ctx->getStart()->getCharPositionInLine());
  return "";
}

Any ASTBuilder::visitCaseStmt(TOPParser::CaseStmtContext *ctx) {
  visit(ctx->expr());
  auto caseExpr = visitedExpr;
  std::vector<std::shared_ptr<ASTCaseArm>> arms;
  for (auto arm : ctx->caseArm()) {
    visit(arm);
    arms.push_back(visitedCaseArm);
  }
  visitedStmt = std::make_shared<ASTCaseStmt>(caseExpr, arms);
  LOG_S(1) << "Built AST node " << *visitedStmt;
  visitedStmt->setLocation(ctx->getStart()->getLine(),
                           ctx->getStart()->getCharPositionInLine());
  return "";
}

Any ASTBuilder::visitAssignStmt(TOPParser::AssignStmtContext *ctx) {
  visit(ctx->expr(0));
  auto lhs = visitedExpr;
  visit(ctx->expr(1));
  auto rhs = visitedExpr;
  visitedStmt = std::make_shared<ASTAssignStmt>(lhs, rhs);

  LOG_S(1) << "Built AST node " << *visitedStmt;

  // Set source location
  visitedStmt->setLocation(ctx->getStart()->getLine(),
                           ctx->getStart()->getCharPositionInLine());
  return "";
}

std::string ASTBuilder::generateSHA256(std::string tohash) {
  std::vector<unsigned char> hash(picosha2::k_digest_size);
  picosha2::hash256(tohash.begin(), tohash.end(), hash.begin(), hash.end());
  return picosha2::bytes_to_hex_string(hash.begin(), hash.end());
}

#pragma once

/*
 * This include is for convenience when defining algorithms spanning
 * the AST type hierarchy, e.g., visitors, code generation, pretty printing.
 *
 * It should be used sparingly as it introduces coupling to the entire
 * AST type hierarchy.
 */

#include "ASTAllocExpr.h"
#include "ASTAssignStmt.h"
#include "ASTBinaryExpr.h"
#include "ASTBlockStmt.h"
#include "ASTBorrowExpr.h"
#include "ASTCaseArm.h"
#include "ASTCaseStmt.h"
#include "ASTCtorPattern.h"
#include "ASTDeRefExpr.h"
#include "ASTDeclNode.h"
#include "ASTDestroyStmt.h"
#include "ASTDeclStmt.h"
#include "ASTErrorStmt.h"
#include "ASTExpr.h"
#include "ASTFunAppExpr.h"
#include "ASTFunction.h"
#include "ASTIfStmt.h"
#include "ASTInputExpr.h"
#include "ASTNode.h"
#include "ASTNumberExpr.h"
#include "ASTOutputStmt.h"
#include "ASTPattern.h"
#include "ASTProgram.h"
#include "ASTBorrowExpr.h"
#include "ASTReturnStmt.h"
#include "ASTStmt.h"
#include "ASTSumCtorExpr.h"
#include "ASTSumTypeDecl.h"
#include "ASTSumVariant.h"
#include "ASTVariableExpr.h"
#include "ASTVarPattern.h"
#include "ASTWhileStmt.h"
#include "ASTWildcardPattern.h"

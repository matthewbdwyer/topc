#include "CodeGenVisitor.h"

#include "AST.h"
#include "ASTCaseArm.h"
#include "ASTCtorPattern.h"
#include "ASTPattern.h"
#include "ASTSumVariant.h"
#include "ASTVarPattern.h"
#include "ASTVisitor.h"
#include "ASTWildcardPattern.h"
#include "CodeGenContext.h"
#include "InternalError.h"
#include "OwnershipClassifier.h"
#include "SemanticAnalysis.h"
#include "TopOwningRef.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/TargetParser/Host.h"
#include "llvm/Transforms/Scalar.h"

#include "loguru.hpp"

#include <algorithm>
#include <map>
#include <stdexcept>

// ---------------------------------------------------------------------------
// File-local helpers (same as before, but use the visitor's ctx_)
// ---------------------------------------------------------------------------
namespace {

llvm::Function *getFunction(const std::string &functionName,
                            CodeGenContext &ctx) {
  auto formalNames = ctx.functionFormalNames[functionName];

  if (functionName == "main") {
    if (auto *M = ctx.module->getFunction("_top_main")) {
      return M;
    }
    ctx.numArgs = formalNames.size();
    auto *fn = llvm::Function::Create(
        llvm::FunctionType::get(llvm::Type::getInt64Ty(ctx.llvmContext), false),
        llvm::Function::ExternalLinkage, "_top_" + functionName,
        ctx.module.get());
    return fn;
  } else {
    if (auto *F = ctx.module->getFunction(functionName)) {
      return F;
    }
    std::vector<llvm::Type *> FormalTypes(formalNames.size(),
                                          llvm::Type::getInt64Ty(ctx.llvmContext));
    auto *FT = llvm::FunctionType::get(
        llvm::Type::getInt64Ty(ctx.llvmContext), FormalTypes, false);
    auto *fn = llvm::Function::Create(FT, llvm::Function::InternalLinkage,
                                      functionName, ctx.module.get());
    unsigned i = 0;
    for (auto &param : fn->args()) {
      param.setName(formalNames[i++]);
    }
    return fn;
  }
}

llvm::AllocaInst *CreateEntryBlockAlloca(llvm::Function *TheFunction,
                                         const std::string &VarName,
                                         CodeGenContext &ctx) {
  llvm::IRBuilder<> tmp(&TheFunction->getEntryBlock(),
                        TheFunction->getEntryBlock().begin());
  return tmp.CreateAlloca(llvm::Type::getInt64Ty(ctx.llvmContext), nullptr, VarName);
}

} // namespace

// ---------------------------------------------------------------------------
// CodeGenVisitor::generate  (top-level entry)
// ---------------------------------------------------------------------------
std::shared_ptr<llvm::Module>
CodeGenVisitor::generate(ASTProgram *program,
                         SemanticAnalysis *semanticAnalysis,
                         const std::string &programName) {
  LOG_S(1) << "Generating code for program " << programName;

  semanticAnalysis_ = semanticAnalysis;

  CodeGenContext ctx;
  ctx_ = &ctx;

  auto TheModule = std::make_shared<llvm::Module>(programName, ctx.llvmContext);

  llvm::Triple targetTriple(llvm::sys::getProcessTriple());
  TheModule->setTargetTriple(targetTriple);

  ctx.nop =
#if LLVM_VERSION_MAJOR >= 18
      llvm::Intrinsic::getOrInsertDeclaration(TheModule.get(),
                                              llvm::Intrinsic::donothing);
#else
      llvm::Intrinsic::getDeclaration(TheModule.get(),
                                      llvm::Intrinsic::donothing);
#endif

  ctx.labelNum = 0;
  ctx.module   = std::move(TheModule);

  {
    int funIndex = 0;
    for (auto const &fn : program->getFunctions()) {
      ctx.functionIndex[fn->getName()] = funIndex++;

      auto formalNames = fn->getFormals();
      std::vector<std::string> names;
      std::transform(formalNames.begin(), formalNames.end(),
                     std::back_inserter(names),
                     [](auto &d) { return d->getName(); });
      ctx.functionFormalNames[fn->getName()] = names;
    }

    std::vector<llvm::Constant *> programFunctions;
    for (auto const &func : program->getFunctions()) {
      programFunctions.emplace_back(getFunction(func->getName(), ctx));
    }

    auto *FunctionOpaquePtrType = llvm::PointerType::get(ctx.llvmContext, 0);
    auto *functionTableType =
        llvm::ArrayType::get(FunctionOpaquePtrType, funIndex);

    std::vector<llvm::Constant *> castProgramFunctions;
    castProgramFunctions.reserve(programFunctions.size());
    for (auto const &pf : programFunctions) {
      castProgramFunctions.push_back(pf);
    }

    auto *ftableInit =
        llvm::ConstantArray::get(functionTableType, castProgramFunctions);

    ctx.topFunctionTable = new llvm::GlobalVariable(
        *ctx.module, functionTableType, true,
        llvm::GlobalValue::InternalLinkage, ftableInit, "_top_ftable");
  }

  {
    auto fidx = ctx.functionIndex.find("main");
    if (fidx == ctx.functionIndex.end()) {
      auto *M = llvm::Function::Create(
          llvm::FunctionType::get(llvm::Type::getInt64Ty(ctx.llvmContext), false),
          llvm::Function::ExternalLinkage, "_top_main", ctx.module.get());
      llvm::BasicBlock *BB =
          llvm::BasicBlock::Create(ctx.llvmContext, "entry", M);
      ctx.irBuilder.SetInsertPoint(BB);

      auto *undef = llvm::Function::Create(
          llvm::FunctionType::get(llvm::Type::getVoidTy(ctx.llvmContext), false),
          llvm::Function::ExternalLinkage, "_top_main_undefined",
          ctx.module.get());
      ctx.irBuilder.CreateCall(undef);
      ctx.irBuilder.CreateRet(
          llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx.llvmContext), 0));
    }

    ctx.topNumInputs = new llvm::GlobalVariable(
        *ctx.module, llvm::Type::getInt64Ty(ctx.llvmContext), true,
        llvm::GlobalValue::ExternalLinkage,
        llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx.llvmContext),
                               ctx.numArgs),
        "_top_num_inputs");

    auto *inputArrayType =
        llvm::ArrayType::get(llvm::Type::getInt64Ty(ctx.llvmContext),
                             ctx.numArgs);
    auto *zeroV =
        llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx.llvmContext), 0);
    std::vector<llvm::Constant *> zeros(ctx.numArgs, zeroV);
    ctx.topInputArray = new llvm::GlobalVariable(
        *ctx.module, inputArrayType, false, llvm::GlobalValue::CommonLinkage,
        llvm::ConstantArray::get(inputArrayType, zeros), "_top_input_array");
  }

  std::vector<llvm::Type *> twoInt(2, llvm::Type::getInt64Ty(ctx.llvmContext));
  auto *FT = llvm::FunctionType::get(
      llvm::PointerType::get(ctx.llvmContext, 0), twoInt, false);
  ctx.callocFun = llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                                         "calloc", ctx.module.get());
  ctx.callocFun->addFnAttr(llvm::Attribute::NoUnwind);
  ctx.callocFun->setAttributes(
      ctx.callocFun->getAttributes().addAttributeAtIndex(
          ctx.callocFun->getContext(), 0, llvm::Attribute::NoAlias));

  for (auto const fn : program->getFunctions()) {
    generate(fn);
  }

  auto resultModule = ctx.bundleModule();
  ctx_ = nullptr;

  verifyModule(*resultModule);
  return resultModule;
}

// ---------------------------------------------------------------------------
// dispatch
// ---------------------------------------------------------------------------
llvm::Value *CodeGenVisitor::dispatch(ASTNode *node) {
  if (node == nullptr) {
    return nullptr;
  }

  class DispatchVisitor final : public ASTVisitor {
  public:
    explicit DispatchVisitor(CodeGenVisitor &codegen) : codegen_(codegen) {}

    llvm::Value *result = nullptr;

    bool visit(ASTFunction *n) override {
      result = codegen_.generate(n);
      return false;
    }
    bool visit(ASTNumberExpr *n) override {
      result = codegen_.generate(n);
      return false;
    }
    bool visit(ASTBinaryExpr *n) override {
      result = codegen_.generate(n);
      return false;
    }
    bool visit(ASTVariableExpr *n) override {
      result = codegen_.generate(n);
      return false;
    }
    bool visit(ASTInputExpr *n) override {
      result = codegen_.generate(n);
      return false;
    }
    bool visit(ASTFunAppExpr *n) override {
      result = codegen_.generate(n);
      return false;
    }
    bool visit(ASTAllocExpr *n) override {
      result = codegen_.generate(n);
      return false;
    }
    bool visit(ASTBorrowExpr *n) override {
      result = codegen_.generate(n);
      return false;
    }
    bool visit(ASTDeRefExpr *n) override {
      result = codegen_.generate(n);
      return false;
    }
    bool visit(ASTDeclNode *n) override {
      result = codegen_.generate(n);
      return false;
    }
    bool visit(ASTDeclStmt *n) override {
      result = codegen_.generate(n);
      return false;
    }
    bool visit(ASTAssignStmt *n) override {
      result = codegen_.generate(n);
      return false;
    }
    bool visit(ASTBlockStmt *n) override {
      result = codegen_.generate(n);
      return false;
    }
    bool visit(ASTWhileStmt *n) override {
      result = codegen_.generate(n);
      return false;
    }
    bool visit(ASTIfStmt *n) override {
      result = codegen_.generate(n);
      return false;
    }
    bool visit(ASTOutputStmt *n) override {
      result = codegen_.generate(n);
      return false;
    }
    bool visit(ASTErrorStmt *n) override {
      result = codegen_.generate(n);
      return false;
    }
    bool visit(ASTReturnStmt *n) override {
      result = codegen_.generate(n);
      return false;
    }
    bool visit(ASTDestroyStmt *n) override {
      result = codegen_.generate(n);
      return false;
    }
    bool visit(ASTSumCtorExpr *n) override {
      result = codegen_.generate(n);
      return false;
    }
    bool visit(ASTCaseStmt *n) override {
      result = codegen_.generate(n);
      return false;
    }

  private:
    CodeGenVisitor &codegen_;
  };

  DispatchVisitor visitor(*this);
  node->accept(&visitor);
  return visitor.result;
}

// ---------------------------------------------------------------------------
// Per-node generate() implementations
// ---------------------------------------------------------------------------

llvm::Value *CodeGenVisitor::generate(ASTFunction *node) {
  LOG_S(1) << "Generating code for " << *node;

  auto &ctx = *ctx_;

  llvm::Function *TheFunction = getFunction(node->getName(), ctx);
  if (TheFunction == nullptr) {
    throw InternalError("failed to declare the function" + // LCOV_EXCL_LINE
                        node->getName());                  // LCOV_EXCL_LINE
  }

  llvm::BasicBlock *BB =
      llvm::BasicBlock::Create(ctx.llvmContext, "entry", TheFunction);
  ctx.irBuilder.SetInsertPoint(BB);
  ctx.namedValues.clear();

  auto *zeroV =
      llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx.llvmContext), 0);

  if (node->getName() == "main") {
    int argIdx = 0;
    for (auto &argName : ctx.functionFormalNames[node->getName()]) {
      llvm::AllocaInst *argAlloc =
          CreateEntryBlockAlloca(TheFunction, argName, ctx);

      std::vector<llvm::Value *> indices;
      indices.push_back(zeroV);
      indices.push_back(
          llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx.llvmContext), argIdx));
      auto *gep = ctx.irBuilder.CreateInBoundsGEP(
          ctx.topInputArray->getValueType(), ctx.topInputArray, indices,
          "inputidx");
      auto *inVal = ctx.irBuilder.CreateLoad(
          llvm::Type::getInt64Ty(ctx.llvmContext), gep,
          "topinput" + std::to_string(argIdx++));
      ctx.irBuilder.CreateStore(inVal, argAlloc);
      ctx.namedValues[argName] = argAlloc;
    }
  } else {
    for (auto &arg : TheFunction->args()) {
      llvm::AllocaInst *argAlloc =
          CreateEntryBlockAlloca(TheFunction, arg.getName().str(), ctx);
      ctx.irBuilder.CreateStore(&arg, argAlloc);
      ctx.namedValues[arg.getName().str()] = argAlloc;
    }
  }

  for (auto const decl : node->getDeclarations()) {
    if (dispatch(decl) == nullptr) {
      TheFunction->eraseFromParent();                    // LCOV_EXCL_LINE
      throw InternalError(                               // LCOV_EXCL_LINE
          "failed to generate bitcode for the function " // LCOV_EXCL_LINE
          "declarations");                               // LCOV_EXCL_LINE
    }
  }

  for (auto stmt : node->getStmts()) {
    if (dispatch(stmt) == nullptr) {
      TheFunction->eraseFromParent();                    // LCOV_EXCL_LINE
      throw InternalError(                               // LCOV_EXCL_LINE
          "failed to generate bitcode for the function " // LCOV_EXCL_LINE
          "statement");                                  // LCOV_EXCL_LINE
    }
  }

  verifyFunction(*TheFunction);
  return TheFunction;
} // LCOV_EXCL_LINE

llvm::Value *CodeGenVisitor::generate(ASTNumberExpr *node) {
  LOG_S(1) << "Generating code for " << *node;
  auto &ctx = *ctx_;
  return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx.llvmContext),
                                node->getValue());
} // LCOV_EXCL_LINE

llvm::Value *CodeGenVisitor::generate(ASTBinaryExpr *node) {
  LOG_S(1) << "Generating code for " << *node;
  auto &ctx = *ctx_;

  llvm::Value *L = dispatch(node->getLeft());
  llvm::Value *R = dispatch(node->getRight());
  if (L == nullptr || R == nullptr) {
    throw InternalError("null binary operand");
  }

  switch (node->getOpKind()) {
  case ASTBinaryExpr::BinaryOp::Add:
    return ctx.irBuilder.CreateAdd(L, R, "add");
  case ASTBinaryExpr::BinaryOp::Sub:
    return ctx.irBuilder.CreateSub(L, R, "subtract");
  case ASTBinaryExpr::BinaryOp::Mul:
    return ctx.irBuilder.CreateMul(L, R, "multiply");
  case ASTBinaryExpr::BinaryOp::Div:
    return ctx.irBuilder.CreateSDiv(L, R, "divide");
  case ASTBinaryExpr::BinaryOp::Gt: {
    auto *cmp = ctx.irBuilder.CreateICmpSGT(L, R, "compare.gt");
    return ctx.irBuilder.CreateIntCast(
        cmp, llvm::IntegerType::getInt64Ty(ctx.llvmContext), false,
        "compare.gt.value");
  }
  case ASTBinaryExpr::BinaryOp::Eq: {
    auto *cmp = ctx.irBuilder.CreateICmpEQ(L, R, "compare.eq");
    return ctx.irBuilder.CreateIntCast(
        cmp, llvm::IntegerType::getInt64Ty(ctx.llvmContext), false,
        "compare.eq.value");
  }
  case ASTBinaryExpr::BinaryOp::Neq: {
    auto *cmp = ctx.irBuilder.CreateICmpNE(L, R, "compare.ne");
    return ctx.irBuilder.CreateIntCast(
        cmp, llvm::IntegerType::getInt64Ty(ctx.llvmContext), false,
        "compare.ne.value");
  }
  }
  // LCOV_EXCL_START
  throw InternalError("Invalid binary operator");
  // LCOV_EXCL_STOP
}

llvm::Value *CodeGenVisitor::generate(ASTVariableExpr *node) {
  LOG_S(1) << "Generating code for " << *node;
  auto &ctx = *ctx_;

  auto nv = ctx.namedValues.find(node->getName());
  if (nv != ctx.namedValues.end()) {
    if (ctx.lValueGen) {
      return ctx.namedValues[nv->first];
    } else {
      return ctx.irBuilder.CreateLoad(nv->second->getAllocatedType(),
                                      nv->second, node->getName().c_str());
    }
  }

  auto fidx = ctx.functionIndex.find(node->getName());
  if (fidx == ctx.functionIndex.end()) {
    throw InternalError("Unknown variable name: " + node->getName());
  }

  return llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx.llvmContext),
                                fidx->second);
}

llvm::Value *CodeGenVisitor::generate(ASTInputExpr *node) {
  LOG_S(1) << "Generating code for " << *node;
  auto &ctx = *ctx_;

  if (ctx.inputIntrinsic == nullptr) {
    auto *FT =
        llvm::FunctionType::get(llvm::Type::getInt64Ty(ctx.llvmContext), false);
    ctx.inputIntrinsic = llvm::Function::Create(
        FT, llvm::Function::ExternalLinkage, "_top_input", ctx.module.get());
  }
  return ctx.irBuilder.CreateCall(ctx.inputIntrinsic);
} // LCOV_EXCL_LINE

llvm::Value *CodeGenVisitor::generate(ASTFunAppExpr *node) {
  LOG_S(1) << "Generating code for " << *node;
  auto &ctx = *ctx_;

  auto *funVal = dispatch(node->getFunction());
  if (funVal == nullptr) {
    throw InternalError("failed to generate bitcode for the function");
  }

  auto *zeroV =
      llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx.llvmContext), 0);

  std::vector<llvm::Value *> indices;
  indices.push_back(zeroV);
  indices.push_back(funVal);

  auto *gep = ctx.irBuilder.CreateInBoundsGEP(
      ctx.topFunctionTable->getValueType(), ctx.topFunctionTable, indices,
      "function.table.slot");

  auto *functionPointer = ctx.irBuilder.CreateLoad(
      llvm::PointerType::get(ctx.llvmContext, 0), gep, "function.ptr");

  std::vector<llvm::Type *> actualTypes(node->getActuals().size(),
                                        llvm::Type::getInt64Ty(ctx.llvmContext));
  auto *funType = llvm::FunctionType::get(llvm::Type::getInt64Ty(ctx.llvmContext),
                                          actualTypes, false);

  std::vector<llvm::Value *> argsV;
  for (auto const arg : node->getActuals()) {
    llvm::Value *argVal = dispatch(arg);
    if (argVal == nullptr) {
      throw InternalError(                                // LCOV_EXCL_LINE
          "failed to generate bitcode for the argument"); // LCOV_EXCL_LINE
    }
    argsV.push_back(argVal);
  }

  return ctx.irBuilder.CreateCall(funType, functionPointer, argsV,
                                  "call.result");
}

llvm::Value *CodeGenVisitor::generate(ASTAllocExpr *node) {
  LOG_S(1) << "Generating code for " << *node;
  auto &ctx = *ctx_;

  ctx.allocFlag = true;
  llvm::Value *argVal = dispatch(node->getInitializer());
  ctx.allocFlag = false;
  if (argVal == nullptr) {
    throw InternalError("failed to generate bitcode for the initializer of the "
                        "alloc expression");
  }

  std::vector<llvm::Value *> twoArg;
  twoArg.push_back(
      llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx.llvmContext), 1));
  twoArg.push_back(
      llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx.llvmContext), 8));
  auto *allocInst = ctx.irBuilder.CreateCall(ctx.callocFun, twoArg, "allocPtr");
  ctx.irBuilder.CreateStore(argVal, allocInst);

  return ctx.irBuilder.CreatePtrToInt(
      allocInst, llvm::Type::getInt64Ty(ctx.llvmContext), "allocIntVal");
}

llvm::Value *CodeGenVisitor::generate(ASTBorrowExpr *node) {
  LOG_S(1) << "Generating code for " << *node;
  auto &ctx = *ctx_;

  ctx.lValueGen = true;
  llvm::Value *lValue = dispatch(node->getVar());
  ctx.lValueGen = false;

  if (lValue == nullptr) {
    throw InternalError("could not generate l-value for address of");
  }

  return ctx.irBuilder.CreatePtrToInt(lValue,
                                      llvm::Type::getInt64Ty(ctx.llvmContext),
                                      "addrOfPtr");
} // LCOV_EXCL_LINE

llvm::Value *CodeGenVisitor::generate(ASTDeRefExpr *node) {
  LOG_S(1) << "Generating code for " << *node;
  auto &ctx = *ctx_;

  bool isLValue = ctx.lValueGen;
  if (isLValue) {
    ctx.lValueGen = false;
  }

  llvm::Value *argVal = dispatch(node->getPtr());
  if (argVal == nullptr) {
    throw InternalError("failed to generate bitcode for the pointer");
  }

  llvm::Value *address = ctx.irBuilder.CreateIntToPtr(
      argVal, llvm::PointerType::get(ctx.llvmContext, 0), "ptrIntVal");

  if (isLValue) {
    return address;
  } else {
    return ctx.irBuilder.CreateLoad(llvm::Type::getInt64Ty(ctx.llvmContext),
                                    address, "valueAt");
  }
}

llvm::Value *CodeGenVisitor::generate(ASTDeclNode *node) {
  throw InternalError("Declarations do not emit code");
}

llvm::Value *CodeGenVisitor::generate(ASTDeclStmt *node) {
  LOG_S(1) << "Generating code for " << *node;
  auto &ctx = *ctx_;

  llvm::Function *TheFunction = ctx.irBuilder.GetInsertBlock()->getParent();
  auto *zeroV =
      llvm::ConstantInt::get(llvm::Type::getInt64Ty(ctx.llvmContext), 0);

  llvm::AllocaInst *localAlloca = nullptr;
  for (auto l : node->getVars()) {
    localAlloca = CreateEntryBlockAlloca(TheFunction, l->getName(), ctx);
    ctx.irBuilder.CreateStore(zeroV, localAlloca);
    ctx.namedValues[l->getName()] = localAlloca;
  }
  return localAlloca;
} // LCOV_EXCL_LINE

llvm::Value *CodeGenVisitor::generate(ASTAssignStmt *node) {
  LOG_S(1) << "Generating code for " << *node;
  auto &ctx = *ctx_;

  ctx.lValueGen = true;
  llvm::Value *lValue = dispatch(node->getLHS());
  ctx.lValueGen = false;

  if (lValue == nullptr) {
    throw InternalError(
        "failed to generate bitcode for the lhs of the assignment");
  }

  bool pointerAssign = dynamic_cast<ASTDeRefExpr *>(node->getLHS()) != nullptr;
  if (pointerAssign) {
    ctx.allocFlag = true;
  }
  llvm::Value *rValue = dispatch(node->getRHS());
  if (pointerAssign) {
    ctx.allocFlag = false;
  }
  if (rValue == nullptr) {
    throw InternalError(
        "failed to generate bitcode for the rhs of the assignment");
  }

  return ctx.irBuilder.CreateStore(rValue, lValue);
} // LCOV_EXCL_LINE

llvm::Value *CodeGenVisitor::generate(ASTBlockStmt *node) {
  LOG_S(1) << "Generating code for " << *node;
  auto &ctx = *ctx_;

  llvm::Value *lastStmt = nullptr;
  for (auto const s : node->getStmts()) {
    lastStmt = dispatch(s);
  }
  return (lastStmt == nullptr) ? ctx.irBuilder.CreateCall(ctx.nop) : lastStmt;
} // LCOV_EXCL_LINE

llvm::Value *CodeGenVisitor::generate(ASTWhileStmt *node) {
  LOG_S(1) << "Generating code for " << *node;
  auto &ctx = *ctx_;

  llvm::Function *TheFunction = ctx.irBuilder.GetInsertBlock()->getParent();
  ctx.labelNum++;

  llvm::BasicBlock *HeaderBB = llvm::BasicBlock::Create(
      ctx.llvmContext, "header" + std::to_string(ctx.labelNum), TheFunction);
  llvm::BasicBlock *BodyBB = llvm::BasicBlock::Create(
      ctx.llvmContext, "body" + std::to_string(ctx.labelNum));
  llvm::BasicBlock *ExitBB = llvm::BasicBlock::Create(
      ctx.llvmContext, "exit" + std::to_string(ctx.labelNum));

  ctx.irBuilder.CreateBr(HeaderBB);

  {
    ctx.irBuilder.SetInsertPoint(HeaderBB);
    llvm::Value *CondV = dispatch(node->getCondition());
    if (CondV == nullptr) {
      throw InternalError(                                   // LCOV_EXCL_LINE
          "failed to generate bitcode for the conditional"); // LCOV_EXCL_LINE
    }
    CondV = ctx.irBuilder.CreateICmpNE(
        CondV, llvm::ConstantInt::get(CondV->getType(), 0), "loopcond");
    ctx.irBuilder.CreateCondBr(CondV, BodyBB, ExitBB);
  }

  {
    TheFunction->insert(TheFunction->end(), BodyBB);
    ctx.irBuilder.SetInsertPoint(BodyBB);
    llvm::Value *BodyV = dispatch(node->getBody());
    if (BodyV == nullptr) {
      throw InternalError(                                 // LCOV_EXCL_LINE
          "failed to generate bitcode for the loop body"); // LCOV_EXCL_LINE
    }
    ctx.irBuilder.CreateBr(HeaderBB);
  }

  TheFunction->insert(TheFunction->end(), ExitBB);
  ctx.irBuilder.SetInsertPoint(ExitBB);
  return ctx.irBuilder.CreateCall(ctx.nop);
} // LCOV_EXCL_LINE

llvm::Value *CodeGenVisitor::generate(ASTIfStmt *node) {
  LOG_S(1) << "Generating code for " << *node;
  auto &ctx = *ctx_;

  llvm::Value *CondV = dispatch(node->getCondition());
  if (CondV == nullptr) {
    throw InternalError(
        "failed to generate bitcode for the condition of the if statement");
  }

  CondV = ctx.irBuilder.CreateICmpNE(
      CondV, llvm::ConstantInt::get(CondV->getType(), 0), "ifcond");

  llvm::Function *TheFunction = ctx.irBuilder.GetInsertBlock()->getParent();
  ctx.labelNum++;

  llvm::BasicBlock *ThenBB = llvm::BasicBlock::Create(
      ctx.llvmContext, "then" + std::to_string(ctx.labelNum), TheFunction);
  llvm::BasicBlock *ElseBB = llvm::BasicBlock::Create(
      ctx.llvmContext, "else" + std::to_string(ctx.labelNum));
  llvm::BasicBlock *MergeBB = llvm::BasicBlock::Create(
      ctx.llvmContext, "ifmerge" + std::to_string(ctx.labelNum));

  ctx.irBuilder.CreateCondBr(CondV, ThenBB, ElseBB);

  {
    ctx.irBuilder.SetInsertPoint(ThenBB);
    llvm::Value *ThenV = dispatch(node->getThen());
    if (ThenV == nullptr) {
      throw InternalError(                                  // LCOV_EXCL_LINE
          "failed to generate bitcode for the then block"); // LCOV_EXCL_LINE
    }
    ctx.irBuilder.CreateBr(MergeBB);
  }

  {
    TheFunction->insert(TheFunction->end(), ElseBB);
    ctx.irBuilder.SetInsertPoint(ElseBB);
    if (node->getElse() != nullptr) {
      llvm::Value *ElseV = dispatch(node->getElse());
      if (ElseV == nullptr) {
        throw InternalError(                                  // LCOV_EXCL_LINE
            "failed to generate bitcode for the else block"); // LCOV_EXCL_LINE
      }
    } else {
      ctx.irBuilder.CreateCall(ctx.nop);
    }
    ctx.irBuilder.CreateBr(MergeBB);
  }

  TheFunction->insert(TheFunction->end(), MergeBB);
  ctx.irBuilder.SetInsertPoint(MergeBB);
  return ctx.irBuilder.CreateCall(ctx.nop);
} // LCOV_EXCL_LINE

llvm::Value *CodeGenVisitor::generate(ASTOutputStmt *node) {
  LOG_S(1) << "Generating code for " << *node;
  auto &ctx = *ctx_;

  if (ctx.outputIntrinsic == nullptr) {
    std::vector<llvm::Type *> oneInt(1, llvm::Type::getInt64Ty(ctx.llvmContext));
    auto *FT = llvm::FunctionType::get(llvm::Type::getInt64Ty(ctx.llvmContext),
                                       oneInt, false);
    ctx.outputIntrinsic = llvm::Function::Create(
        FT, llvm::Function::ExternalLinkage, "_top_output", ctx.module.get());
  }

  llvm::Value *argVal = dispatch(node->getArg());
  if (argVal == nullptr) {
    throw InternalError(
        "failed to generate bitcode for the argument of the output statement");
  }

  std::vector<llvm::Value *> ArgsV(1, argVal);
  return ctx.irBuilder.CreateCall(ctx.outputIntrinsic, ArgsV);
}

llvm::Value *CodeGenVisitor::generate(ASTErrorStmt *node) {
  LOG_S(1) << "Generating code for " << *node;
  auto &ctx = *ctx_;

  if (ctx.errorIntrinsic == nullptr) {
    std::vector<llvm::Type *> oneInt(1, llvm::Type::getInt64Ty(ctx.llvmContext));
    auto *FT = llvm::FunctionType::get(llvm::Type::getInt64Ty(ctx.llvmContext),
                                       oneInt, false);
    ctx.errorIntrinsic = llvm::Function::Create(
        FT, llvm::Function::ExternalLinkage, "_top_error", ctx.module.get());
  }

  llvm::Value *argVal = dispatch(node->getArg());
  if (argVal == nullptr) {
    throw InternalError(
        "failed to generate bitcode for the argument of the error statement");
  }

  std::vector<llvm::Value *> ArgsV(1, argVal);
  return ctx.irBuilder.CreateCall(ctx.errorIntrinsic, ArgsV);
}

llvm::Value *CodeGenVisitor::generate(ASTReturnStmt *node) {
  LOG_S(1) << "Generating code for " << *node;
  auto &ctx = *ctx_;
  llvm::Value *argVal = dispatch(node->getArg());
  return ctx.irBuilder.CreateRet(argVal);
}

// ---------------------------------------------------------------------------
// Destruction: free owned heap resources (Phase 11)
// ---------------------------------------------------------------------------

void CodeGenVisitor::ensureFreeDecl(CodeGenContext &ctx) {
  if (ctx.freeFun)
    return;
  auto *voidTy = llvm::Type::getVoidTy(ctx.llvmContext);
  auto *ptrTy  = llvm::PointerType::get(ctx.llvmContext, 0);
  auto *FT     = llvm::FunctionType::get(voidTy, {ptrTy}, false);
  ctx.freeFun  = llvm::Function::Create(FT, llvm::Function::ExternalLinkage,
                                        "free", ctx.module.get());
  ctx.freeFun->addFnAttr(llvm::Attribute::NoUnwind);
}


void CodeGenVisitor::emitDestroyValue(llvm::Value *ptrAsInt, TopType *topType,
                                       CodeGenContext &ctx) {
  auto *owningRef = dynamic_cast<TopOwningRef *>(topType);
  if (owningRef == nullptr) {
    return; // Copy type — nothing to free
  }

  auto *ptrTy  = llvm::PointerType::get(ctx.llvmContext, 0);
  auto *i64Ty  = llvm::Type::getInt64Ty(ctx.llvmContext);
  auto *i32Ty  = llvm::Type::getInt32Ty(ctx.llvmContext);
  auto *zeroV  = llvm::ConstantInt::get(i64Ty, 0);

  // Convert the int64-encoded pointer to an actual pointer.
  auto *ptr = ctx.irBuilder.CreateIntToPtr(ptrAsInt, ptrTy, "destroyPtr");

  // Free the pointer itself.
  ctx.irBuilder.CreateCall(ctx.freeFun, {ptr});
}

llvm::Value *CodeGenVisitor::generate(ASTDestroyStmt *node) {
  LOG_S(1) << "Generating code for " << *node;
  auto &ctx = *ctx_;

  ensureFreeDecl(ctx);

  ASTDeclNode *decl    = node->getVar();
  std::string varName  = decl->getName();

  auto it = ctx.namedValues.find(varName);
  if (it == ctx.namedValues.end()) {
    throw InternalError("destroy: variable '" + varName + // LCOV_EXCL_LINE
                        "' not found in namedValues");    // LCOV_EXCL_LINE
  }

  // Load the i64 pointer-as-int stored in the variable's alloca.
  auto *i64Ty    = llvm::Type::getInt64Ty(ctx.llvmContext);
  auto *ptrAsInt = ctx.irBuilder.CreateLoad(i64Ty, it->second, varName + ".val");

  // Look up the variable's inferred type for recursive destruction.
  // Keep the shared_ptr alive for the duration of emitDestroyValue.
  auto topTypeShared = semanticAnalysis_->getTypeResults()->getInferredType(decl);
  TopType *topType = topTypeShared.get();

  emitDestroyValue(ptrAsInt, topType, ctx);

  return ctx.irBuilder.CreateCall(ctx.nop);
}

// ---------------------------------------------------------------------------
// Pattern matching helper (Phase B4)
// ---------------------------------------------------------------------------

void CodeGenVisitor::emitPatternMatch(llvm::Value *basePtr, int64_t offset,
                                       ASTPattern *pat, ASTDeclNode *paramDecl,
                                       llvm::BasicBlock *failBB,
                                       llvm::Function *func,
                                       CodeGenContext &ctx) {
  auto &builder = ctx.irBuilder;
  auto *i64Ty   = llvm::Type::getInt64Ty(ctx.llvmContext);
  auto *ptrTy   = llvm::PointerType::get(ctx.llvmContext, 0);

  // Load the field value at the given offset within basePtr.
  auto *fieldGEP = builder.CreateInBoundsGEP(
      i64Ty, basePtr, {llvm::ConstantInt::get(i64Ty, offset)}, "pat.gep");
  auto *fieldVal = builder.CreateLoad(i64Ty, fieldGEP, "pat.val");

  if (auto *vp = dynamic_cast<ASTVarPattern *>(pat)) {
    // Variable pattern: bind field value to the variable's stack slot.
    auto *alloca = CreateEntryBlockAlloca(func, vp->getName(), ctx);
    builder.CreateStore(fieldVal, alloca);
    ctx.namedValues[vp->getName()] = alloca;

  } else if (dynamic_cast<ASTWildcardPattern *>(pat)) {
    // Wildcard: if the payload slot is Own, destroy it (prevents leaks).
    if (paramDecl) {
      auto typeShared =
          semanticAnalysis_->getTypeResults()->getInferredType(paramDecl);
      if (typeShared &&
          OwnershipClassifier::classifyType(typeShared.get()) ==
              OwnershipClass::Own) {
        ensureFreeDecl(ctx);
        emitDestroyValue(fieldVal, typeShared.get(), ctx);
      }
    }

  } else if (auto *cp = dynamic_cast<ASTCtorPattern *>(pat)) {
    // Nested constructor pattern: extract inner sum-type pointer, check tag.
    auto *innerPtr = builder.CreateIntToPtr(fieldVal, ptrTy, "inner.ptr");
    auto *innerTag = builder.CreateLoad(i64Ty, innerPtr, "inner.tag");

    // Resolve the inner constructor's tag index and parameter list.
    auto *symTab          = semanticAnalysis_->getSymbolTable();
    auto *innerOwnerDecl  = symTab->getConstructorOwner(cp->getTag());
    int   innerTagIdx     = 0;
    std::vector<ASTDeclNode *> innerParams;
    if (innerOwnerDecl) {
      int idx = 0;
      for (auto *v : innerOwnerDecl->getVariants()) {
        if (v->getTag() == cp->getTag()) {
          innerTagIdx  = idx;
          innerParams  = v->getParams();
          break;
        }
        ++idx;
      }
    }

    // Conditional branch: jump to matchBB if the inner tag matches, failBB if not.
    auto *expected = llvm::ConstantInt::get(i64Ty, innerTagIdx);
    auto *cond     = builder.CreateICmpEQ(innerTag, expected, "ctor.cmp");
    auto *matchBB  = llvm::BasicBlock::Create(ctx.llvmContext, "ctor.ok", func);
    builder.CreateCondBr(cond, matchBB, failBB);
    builder.SetInsertPoint(matchBB);

    // Recurse: match sub-patterns against the inner struct's payload slots.
    auto subPats = cp->getSubPatterns();
    for (std::size_t j = 0; j < subPats.size(); ++j) {
      ASTDeclNode *subDecl =
          (j < innerParams.size()) ? innerParams[j] : nullptr;
      emitPatternMatch(innerPtr, static_cast<int64_t>(j + 1), subPats[j],
                       subDecl, failBB, func, ctx);
    }

  }
}

llvm::Value *CodeGenVisitor::generate(ASTSumCtorExpr *node) {
  LOG_S(1) << "Generating code for " << *node;
  auto &ctx = *ctx_;
  auto *i64Ty = llvm::Type::getInt64Ty(ctx.llvmContext);

  // Determine tag index and arity from the symbol table.
  auto *symTab = semanticAnalysis_->getSymbolTable();
  auto *ownerDecl = symTab->getConstructorOwner(node->getTag());

  int tagIdx = 0;
  int arity = 0;
  if (ownerDecl) {
    int idx = 0;
    for (auto *v : ownerDecl->getVariants()) {
      if (v->getTag() == node->getTag()) {
        tagIdx = idx;
        arity = static_cast<int>(v->getParams().size());
        break;
      }
      idx++;
    }
  }

  // Evaluate argument expressions before allocating memory.
  std::vector<llvm::Value *> argVals;
  for (auto *arg : node->getArgs()) {
    argVals.push_back(dispatch(arg));
  }

  // calloc(1, (1 + arity) * 8) — zero-initialized block.
  std::vector<llvm::Value *> callocArgs = {
      llvm::ConstantInt::get(i64Ty, 1),
      llvm::ConstantInt::get(i64Ty, (1 + arity) * 8)};
  auto *allocPtr =
      ctx.irBuilder.CreateCall(ctx.callocFun, callocArgs, "constructor.ptr");

  // Store the tag index at offset 0.
  ctx.irBuilder.CreateStore(llvm::ConstantInt::get(i64Ty, tagIdx), allocPtr);

  // Store each argument value at successive i64 offsets.
  for (int i = 0; i < static_cast<int>(argVals.size()); i++) {
    auto *fieldPtr = ctx.irBuilder.CreateInBoundsGEP(
        i64Ty, allocPtr,
        {llvm::ConstantInt::get(i64Ty, i + 1)},
        "constructor.payload." + std::to_string(i));
    ctx.irBuilder.CreateStore(argVals[i], fieldPtr);
  }

  return ctx.irBuilder.CreatePtrToInt(allocPtr, i64Ty, "constructor.value");
}

llvm::Value *CodeGenVisitor::generate(ASTCaseStmt *node) {
  LOG_S(1) << "Generating code for " << *node;
  auto &ctx = *ctx_;
  auto *i64Ty = llvm::Type::getInt64Ty(ctx.llvmContext);

  // Evaluate the case expression (an i64 pointer-as-int).
  llvm::Value *caseExprInt = dispatch(node->getCaseExpr());

  // Recover the actual pointer.
    auto *caseExprPtr = ctx.irBuilder.CreateIntToPtr(
      caseExprInt, llvm::PointerType::get(ctx.llvmContext, 0), "case.value.ptr");

  // Load the tag (first i64 in the sum-type allocation).
  auto *tagVal = ctx.irBuilder.CreateLoad(i64Ty, caseExprPtr, "tag");

  // Build a variant-name → tag-index map and variant-name → params map.
  auto arms    = node->getArms();
  auto *symTab = semanticAnalysis_->getSymbolTable();
  auto *ownerDecl = symTab->getConstructorOwner(arms[0]->getTag());

  std::map<std::string, int>                    variantIndex;
  std::map<std::string, std::vector<ASTDeclNode *>> variantParams;
  if (ownerDecl) {
    int idx = 0;
    for (auto *v : ownerDecl->getVariants()) {
      variantIndex[v->getTag()]  = idx++;
      variantParams[v->getTag()] = v->getParams();
    }
  }

  llvm::Function *TheFunction = ctx.irBuilder.GetInsertBlock()->getParent();
  ctx.labelNum++;
  int label = ctx.labelNum;

  // Merge block (fall-through destination for all arms).
  llvm::BasicBlock *MergeBB = llvm::BasicBlock::Create(
      ctx.llvmContext, "case.merge." + std::to_string(label));

  // Default block for the LLVM switch (unreachable in well-typed programs).
  llvm::BasicBlock *DefaultBB = llvm::BasicBlock::Create(
      ctx.llvmContext, "case.default." + std::to_string(label));

  // Group arms by outer constructor tag, preserving declaration order.
  std::map<std::string, std::vector<ASTCaseArm *>> groupedArms;
  std::vector<std::string> tagOrder;
  for (auto *arm : arms) {
    if (groupedArms.find(arm->getTag()) == groupedArms.end())
      tagOrder.push_back(arm->getTag());
    groupedArms[arm->getTag()].push_back(arm);
  }

  // One LLVM switch case per distinct outer tag.
  auto *switchInst = ctx.irBuilder.CreateSwitch(
      tagVal, DefaultBB, static_cast<unsigned>(tagOrder.size()));

  // Emit one chain of arm checks per distinct outer tag.
  for (const auto &tag : tagOrder) {
    int tagIdx = variantIndex.count(tag) ? variantIndex.at(tag) : 0;
    const auto &armGroup = groupedArms.at(tag);
    const auto &params   = variantParams.count(tag)
                               ? variantParams.at(tag)
                               : std::vector<ASTDeclNode *>{};

    // Entry block for this outer tag (the switch target).
    auto *entryBB = llvm::BasicBlock::Create(
        ctx.llvmContext,
        "case." + tag + "." + std::to_string(label),
        TheFunction);
    switchInst->addCase(llvm::ConstantInt::get(i64Ty, tagIdx), entryBB);
    ctx.irBuilder.SetInsertPoint(entryBB);

    // Emit each arm in the group as a sequential chain.
    for (std::size_t ai = 0; ai < armGroup.size(); ++ai) {
      auto *arm = armGroup[ai];

      // failBB: where to go if this arm's nested patterns don't match.
      // For the last arm in a group the default block is used (unreachable
      // in well-typed programs after B3 redundancy checks).
      llvm::BasicBlock *failBB;
      if (ai + 1 < armGroup.size()) {
        failBB = llvm::BasicBlock::Create(
            ctx.llvmContext,
            "case." + tag + ".arm" + std::to_string(ai + 1) + "." +
                std::to_string(label));
      } else {
        failBB = DefaultBB;
      }

      // Emit pattern bindings for each payload position of this arm.
      auto patterns = arm->getPatterns();
      for (std::size_t pi = 0; pi < patterns.size(); ++pi) {
        ASTDeclNode *paramDecl =
            (pi < params.size()) ? params[pi] : nullptr;
        emitPatternMatch(caseExprPtr, static_cast<int64_t>(pi + 1),
                         patterns[pi], paramDecl, failBB, TheFunction, ctx);
      }

      // Generate the arm body (all pattern tests passed).
      dispatch(arm->getBody());

      // Branch to merge unless the body already terminated the block.
      if (!ctx.irBuilder.GetInsertBlock()->getTerminator())
        ctx.irBuilder.CreateBr(MergeBB);

      // Remove arm's named bindings so they don't bleed into sibling arms.
      for (auto *b : arm->getBindings())
        ctx.namedValues.erase(b->getName());

      // If there is a next arm, its failBB is now the new insert point.
      if (ai + 1 < armGroup.size()) {
        TheFunction->insert(TheFunction->end(), failBB);
        ctx.irBuilder.SetInsertPoint(failBB);
      }
    }
  }

  // Fill the default block.
  TheFunction->insert(TheFunction->end(), DefaultBB);
  ctx.irBuilder.SetInsertPoint(DefaultBB);
  ctx.irBuilder.CreateCall(ctx.nop);
  ctx.irBuilder.CreateBr(MergeBB);

  // Continue after the case statement.
  TheFunction->insert(TheFunction->end(), MergeBB);
  ctx.irBuilder.SetInsertPoint(MergeBB);
  return ctx.irBuilder.CreateCall(ctx.nop);
}



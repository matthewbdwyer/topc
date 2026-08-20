#pragma once

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

/*! \struct CodeGenContext
 *  \brief Holds all per-compilation state for TOP code generation.
 *
 * Replacing the 22 file-scope globals that lived in the anonymous namespace of
 * CodeGenFunctions.cpp.  Constructing a fresh CodeGenContext for each
 * compilation guarantees that no state bleeds between compilations.
 *
 * LIFETIME NOTE: The LLVMContext must outlive any llvm::Module that was
 * created from it.  CodeGenContext owns both; callers that need the Module
 * must transfer ownership together with the context via the helper
 * bundleModule() below.
 */
struct CodeGenContext {
  // The LLVM context is heap-allocated so that ownership can be shared with
  // the returned Module (see bundleModule below).
  std::shared_ptr<llvm::LLVMContext> llvmContextPtr =
      std::make_shared<llvm::LLVMContext>();
  llvm::LLVMContext &llvmContext = *llvmContextPtr;

  llvm::IRBuilder<> irBuilder{llvmContext};

  std::shared_ptr<llvm::Module> module;

  // Per-program function dispatch state
  std::map<std::string, int>                      functionIndex;
  std::map<std::string, std::vector<std::string>> functionFormalNames;

  // Per-function local variable bindings
  std::map<std::string, llvm::AllocaInst *> namedValues;

  // Lazily generated per-sum-type recursive destroy functions, keyed by the
  // sum type name (e.g. "List" -> @_top_destroy_List).
  std::map<std::string, llvm::Function *> sumDestroyFns;

  // Global variables emitted into the module
  llvm::GlobalVariable *topFunctionTable = nullptr;
  llvm::GlobalVariable *topNumInputs     = nullptr;
  llvm::GlobalVariable *topInputArray    = nullptr;

  // Intrinsic / runtime functions
  llvm::Function *nop             = nullptr;
  llvm::Function *inputIntrinsic  = nullptr;
  llvm::Function *outputIntrinsic = nullptr;
  llvm::Function *errorIntrinsic  = nullptr;
  llvm::Function *callocFun       = nullptr;
  llvm::Function *freeFun         = nullptr;

  // Counters and flags
  int     labelNum = 0;
  int64_t numArgs  = 0;
  bool    lValueGen = false;
  bool    allocFlag  = false;

  // Non-copyable.
  CodeGenContext()                               = default;
  CodeGenContext(const CodeGenContext &)         = delete;
  CodeGenContext &operator=(const CodeGenContext &) = delete;

  /*! \fn bundleModule
   *  \brief Extract the module while keeping the LLVMContext alive.
   *
   * Returns a shared_ptr<Module> that carries a shared reference to the
   * underlying LLVMContext via a custom deleter, guaranteeing the context
   * outlives the module.
   */
  std::shared_ptr<llvm::Module> bundleModule() {
    auto ctxRef = llvmContextPtr;  // shared ownership of context
    auto mod    = module;          // shared ownership of module
    // Wrap in a new shared_ptr that keeps ctxRef alive through a custom owner.
    // We allocate a small helper struct that holds both shared_ptrs; the
    // returned pointer aliases the Module inside it.
    struct Owner {
      std::shared_ptr<llvm::LLVMContext> ctx;
      std::shared_ptr<llvm::Module>      mod;
    };
    auto owner = std::make_shared<Owner>(Owner{ctxRef, mod});
    // Aliasing constructor: control block from owner, pointer from mod.get()
    return std::shared_ptr<llvm::Module>(owner, owner->mod.get());
  }
};


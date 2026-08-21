#include "CodeGenerator.h"
#include "FrontEnd.h"
#include "InternalError.h"
#include "Optimizer.h"
#include "ParseError.h"
#include "SemanticAnalysis.h"
#include "ConstraintRenderer.h"
#include "SemanticError.h"
#include "TypeConstraintCollectVisitor.h"
#include "SyntaxTree.h"
#include "CheckAllocPayload.h"
#include "CheckAssignable.h"
#include "CheckBorrowPositions.h"
#include "CheckCaseCompleteness.h"
#include "CheckPatternTypes.h"
#include "CheckSumTypeNames.h"
#include "iterators/Iterator.h"
#include "ASTBorrowExpr.h"
#include "ASTDestroyStmt.h"
#include "ASTFunAppExpr.h"
#include "ASTFunction.h"
#include "ASTProgram.h"
#include "ASTVariableExpr.h"
#include "BorrowChecker.h"
#include "MoveAnalysis.h"
#include "cfg/CFGRenderer.h"
#include "cfg/IntraproceduralCFGs.h"
#include "symboltable/SymbolTable.h"
#include "types/TypeInference.h"
#include "cfa/CallGraph.h"
#include "OwnershipClassifier.h"
#include "DestructionPass.h"
#include "loguru.hpp"
#include "llvm/Support/CommandLine.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>

using namespace llvm;
using namespace std;

static cl::OptionCategory
    TOPcat("topc Options",
           "Options for controlling the TOP compilation process.");
static cl::opt<bool> psource("psource", cl::desc("print normalized source"),
               cl::cat(TOPcat));
static cl::opt<std::string>
  past("past", cl::value_desc("dot|ascii"), cl::ValueOptional,
     cl::desc("print source AST (default format: dot)"),
     cl::init("dot"), cl::cat(TOPcat));
static cl::opt<bool> psym("psym", cl::desc("print symbols and scopes"),
              cl::cat(TOPcat));
static cl::opt<bool> ptype("ptype", cl::desc("print inferred types"),
               cl::cat(TOPcat));
static cl::opt<std::string>
  pcallgraph("pcallgraph", cl::value_desc("dot|ascii"), cl::ValueOptional,
         cl::desc("print call graph result (default format: dot)"),
         cl::init("dot"), cl::cat(TOPcat));
static cl::opt<std::string>
  pcfg("pcfg", cl::value_desc("dot|ascii"), cl::ValueOptional,
     cl::desc("print source CFG result (default format: dot)"),
     cl::init("dot"), cl::cat(TOPcat));
static cl::opt<bool> pownership("pownership",
                cl::desc("print ownership analysis result"),
                cl::cat(TOPcat));
static cl::opt<bool> pborrow("pborrow",
               cl::desc("print borrow validity result"),
               cl::cat(TOPcat));
static cl::opt<bool> constraint("constraint",
                cl::desc("include constraint/trace details"),
                cl::cat(TOPcat));
static cl::opt<std::string>
  outputDir("output-dir", cl::value_desc("directory"),
        cl::desc("output directory for graph artifacts"),
        cl::init(""), cl::cat(TOPcat));
static cl::opt<bool> disopt("do", cl::desc("disable bitcode optimization"),
                            cl::cat(TOPcat));
static cl::opt<bool> emitSan("san",
                             cl::desc("instrument generated IR with Address/LeakSanitizer"),
                             cl::cat(TOPcat));
static cl::alias emitSanAlias("asan", cl::desc("deprecated alias for --san"),
                              cl::aliasopt(emitSan));
static cl::opt<int> debug(
    "verbose",
  cl::desc("enable log messages (levels 1-3)\n Level 1 - semantic phase "
       "lifecycle and summaries.\n Level 2 - Level 1 and per-declaration "
       "semantic decisions.\n Level 3 - Level 2 and constraints, solver, "
       "and dataflow mechanics."),
    cl::cat(TOPcat));
static cl::opt<bool>
    emitHrAsm("asm", cl::desc("emit human-readable LLVM assembly language"),
              cl::cat(TOPcat));
static cl::opt<std::string>
    logfile("log", cl::value_desc("logfile"),
            cl::desc("log all messages to logfile (enables --verbose 3)"),
            cl::cat(TOPcat));
static cl::opt<std::string> sourceFile(cl::Positional,
                                       cl::desc("<top source file>"),
                                       cl::Required, cl::cat(TOPcat));
static cl::opt<std::string> outputfile("o", cl::value_desc("outputfile"),
                                       cl::desc("write output to <outputfile>"),
                                       cl::cat(TOPcat));

namespace {
bool isValidGraphFormat(const std::string &f) {
  return f == "dot" || f == "ascii";
}

std::string selectedFormat(const cl::opt<std::string> &option) {
  return option.getNumOccurrences() > 0 ? option.getValue() : "";
}

std::string resolveGraphPath(const std::string &source,
                             const std::string &suffix,
                             const std::string &directory) {
  namespace fs = std::filesystem;
  if (directory.empty()) {
    return source + suffix;
  }

  fs::path outDir(directory);
  std::error_code ec;
  fs::create_directories(outDir, ec);
  if (ec) {
    throw InternalError("failed to create output directory '" + directory +
                        "': " + ec.message());
  }
  return (outDir / (fs::path(source).filename().string() + suffix)).string();
}

void writeCallGraphAscii(ASTProgram *ast, CallGraph *callGraph,
                         std::ostream &os) {
  os << "[callgraph]\n";
  for (auto *fn : ast->getFunctions()) {
    std::vector<std::string> names;
    for (auto *callee : callGraph->getCallees(fn)) {
      names.push_back(callee->getName());
    }
    std::sort(names.begin(), names.end());

    os << "  " << fn->getName() << " -> {";
    for (std::size_t i = 0; i < names.size(); ++i) {
      if (i > 0) {
        os << ", ";
      }
      os << names[i];
    }
    os << "}\n";
  }
}

void printOwnershipResult(ASTProgram *ast, SymbolTable *symbols,
                          OwnershipClassifier *classifier, std::ostream &os) {
  os << "[ownership-result]\n";
  for (auto *function : symbols->getFunctions()) {
    const auto cls = classifier->classify(function);
    os << "  function " << function->getName() << " : "
       << (cls == OwnershipClass::Own ? "Own" : "Copy") << "\n";
    for (auto *local : symbols->getLocals(function)) {
      const auto localCls = classifier->classify(local);
      os << "  local " << function->getName() << "." << local->getName()
         << " : " << (localCls == OwnershipClass::Own ? "Own" : "Copy")
         << "\n";
    }
  }

  os << "[destruction-summary]\n";
  for (auto *function : ast->getFunctions()) {
    int count = 0;
    for (auto *stmt : function->getStmts()) {
      if (dynamic_cast<ASTDestroyStmt *>(stmt) != nullptr) {
        count++;
      }
    }
    os << "  " << function->getName() << " : " << count << " destroy"
       << (count == 1 ? "" : "s") << "\n";
  }
}

void printBorrowResult(std::ostream &os) {
  os << "[borrow-result]\n";
  for (const auto &event : BorrowChecker::getLastTrace()) {
    os << "  " << ConstraintRenderer::formatSpan(event.line, event.column)
       << " " << event.expr << " -> "
       << (event.approved ? "approved" : "rejected") << "\n";
  }
}

std::vector<ConstraintRecord>
collectTypeConstraintRecords(ASTProgram *ast, SymbolTable *symbols) {
  TypeConstraintCollectVisitor visitor(symbols);
  ast->accept(&visitor);

  std::vector<ConstraintRecord> records;
  for (const auto &constraint : visitor.getCollectedConstraints()) {
    std::ostringstream text;
    text << constraint;
    records.push_back({"type", 0, 0, text.str()});
  }
  return records;
}

std::vector<ConstraintRecord>
collectTypeSchemeRecords(SymbolTable *symbols, TypeInference *typeResults) {
  std::vector<ConstraintRecord> records;
  for (auto *function : symbols->getFunctions()) {
    if (!symbols->getPoly(function->getName())) {
      continue;
    }
    std::ostringstream text;
    text << function->getName() << " : "
         << *typeResults->getInferredType(function);
    records.push_back(
        {"scheme", function->getLine(), function->getColumn(), text.str()});
  }
  return records;
}

std::vector<ConstraintRecord> collectTypeInstantiationRecords(ASTProgram *ast,
                                                              SymbolTable *symbols,
                                                              CallGraph *cg) {
  std::vector<ConstraintRecord> records;
  SyntaxTree tree(std::shared_ptr<ASTNode>(ast, [](ASTNode *) {}));
  for (auto it = tree.begin(""); it != tree.end(""); ++it) {
    auto node = it->getRoot().get();
    auto *call = dynamic_cast<ASTFunAppExpr *>(node);
    if (!call) {
      continue;
    }

    auto *calleeExpr = dynamic_cast<ASTVariableExpr *>(call->getFunction());
    const bool isDirectNamedCall =
        calleeExpr != nullptr &&
        symbols->getFunction(calleeExpr->getName()) != nullptr;
    if (!isDirectNamedCall) {
      continue;
    }

    std::set<std::string> polyTargets;
    for (auto *called : cg->getCalledFuns(call)) {
      if (symbols->getPoly(called->getName())) {
        polyTargets.insert(called->getName());
      }
    }

    for (const auto &target : polyTargets) {
      std::ostringstream text;
      text << "call " << *call << " instantiates " << target;
      records.push_back({"instantiation", call->getLine(), call->getColumn(),
                         text.str()});
    }
  }
  return records;
}

std::vector<ConstraintRecord>
collectInferredTypeRecords(SymbolTable *symbols, TypeInference *typeResults) {
  std::vector<ConstraintRecord> records;
  for (const auto &name : symbols->getSumTypes()) {
    auto *type = symbols->getSumType(name);
    std::ostringstream text;
    text << "type " << name << " : "
         << typeResults->getInferredTypeDisplay(type);
    records.push_back(
        {"inferred", type->getLine(), type->getColumn(), text.str()});
  }
  for (auto *function : symbols->getFunctions()) {
    {
      std::ostringstream text;
      text << "function " << function->getName() << " : "
           << *typeResults->getInferredType(function);
      records.push_back(
          {"inferred", function->getLine(), function->getColumn(), text.str()});
    }
    for (auto *local : symbols->getLocals(function)) {
      std::ostringstream text;
      text << "local " << function->getName() << "." << local->getName()
           << " : " << *typeResults->getInferredType(local);
      records.push_back({"inferred", local->getLine(), local->getColumn(),
                         text.str()});
    }
  }
  return records;
}

void printTypeConstraints(ASTProgram *ast, SymbolTable *symbols,
                          TypeInference *typeResults, CallGraph *cg,
                          std::ostream &os) {
  ConstraintRenderer::renderSection(
      "type-constraints", collectTypeConstraintRecords(ast, symbols), os);
  ConstraintRenderer::renderSection(
      "type-schemes", collectTypeSchemeRecords(symbols, typeResults), os);
  ConstraintRenderer::renderSection(
      "type-instantiations",
      collectTypeInstantiationRecords(ast, symbols, cg), os);
  ConstraintRenderer::renderSection(
      "type-inferred", collectInferredTypeRecords(symbols, typeResults), os);
}

void printCallGraphConstraints(ASTProgram *ast, CallGraph *cg, std::ostream &os) {
  (void)ast;
  std::vector<ConstraintRecord> records;
  for (const auto &record : cg->getConstraintRecords()) {
    std::ostringstream text;
    text << "caller " << record.callerName << " call " << *record.call
         << " -> {";
    bool first = true;
    for (auto *f : record.targets) {
      if (!first)
        text << ", ";
      text << f->getName();
      first = false;
    }
    text << "}";
    records.push_back(
        {"call-target", record.call->getLine(), record.call->getColumn(),
         text.str()});
  }
  ConstraintRenderer::renderSection("cg-constraints", records, os);
}

void printOwnershipConstraints(SymbolTable *symbols, OwnershipClassifier *oc,
                               std::ostream &os) {
  std::vector<ConstraintRecord> records;
  for (auto *f : symbols->getFunctions()) {
    auto c = oc->classify(f);
    std::ostringstream functionText;
    functionText << "function " << f->getName() << " : "
                 << (c == OwnershipClass::Own ? "Own" : "Copy");
    records.push_back(
        {"ownership", f->getLine(), f->getColumn(), functionText.str()});
    for (auto *l : symbols->getLocals(f)) {
      auto lc = oc->classify(l);
      std::ostringstream localText;
      localText << "local " << f->getName() << "." << l->getName() << " : "
                << (lc == OwnershipClass::Own ? "Own" : "Copy");
      records.push_back({"ownership", l->getLine(), l->getColumn(),
                         localText.str()});
    }
  }

  for (const auto &event : MoveAnalysis::getLastTrace()) {
    std::ostringstream text;
    text << event.kind << " " << event.variable << " (" << event.detail
         << ")";
    records.push_back({"move", event.line, 1, text.str()});
  }

  ConstraintRenderer::renderSection("ownership-constraints", records, os);
}

void printBorrowConstraints(ASTProgram *ast, std::ostream &os) {
  (void)ast;
  std::vector<ConstraintRecord> records;
  for (const auto &event : BorrowChecker::getLastProvenance()) {
    std::ostringstream text;
    std::string label;
    if (event.kind == BorrowChecker::BorrowProvenanceEvent::Kind::Direct) {
      label = "borrow";
      text << event.originExpr << " -> approved; direct argument "
           << event.argumentIndex << " of " << event.callee;
    } else {
      label = "borrow-flow";
      text << "hop " << event.hop << " " << event.expression
           << " -> argument " << event.argumentIndex << " of " << event.callee
           << " at "
           << ConstraintRenderer::formatSpan(event.useLine, event.useColumn);
    }
    records.push_back(
        {label, event.originLine, event.originColumn, text.str()});
  }
  ConstraintRenderer::renderSection("borrow-constraints", records, os);
}
} // namespace

/*! \brief topc driver.
 *
 * This function is the entry point for topc.   It handles command line parsing
 * using LLVM CommandLine support.  It runs the phases of the compiler in
 * sequence. If an error is detected, via an exception, it reports the error and
 * exits. If there is no error, then the LLVM bitcode is emitted to a file whose
 * name is the provided source file suffixed by ".bc".
 */
int main(int argc, char *argv[]) {
  cl::HideUnrelatedOptions(TOPcat);
  cl::ParseCommandLineOptions(argc, argv, "topc - a TOP to llvm compiler\n");

  const bool wantsSource = psource.getValue();
  const bool wantsAst = past.getNumOccurrences() > 0;
  const bool wantsSymbols = psym.getValue();
  const bool wantsTypes = ptype.getValue();
  const bool wantsCallGraph = pcallgraph.getNumOccurrences() > 0;
  const bool wantsCFG = pcfg.getNumOccurrences() > 0;
  const bool wantsOwnership = pownership.getValue();
  const bool wantsBorrow = pborrow.getValue();
  const bool wantsConstraint = constraint.getValue();

  const std::string astFormat = selectedFormat(past);
  const std::string callGraphFormat = selectedFormat(pcallgraph);
  const std::string cfgFormat = selectedFormat(pcfg);

  if (!astFormat.empty() && !isValidGraphFormat(astFormat)) {
    std::cerr << "topc: error: invalid --past format '" << astFormat
              << "' (expected: dot|ascii)\n";
    std::exit(EXIT_FAILURE);
  }
  if (!callGraphFormat.empty() && !isValidGraphFormat(callGraphFormat)) {
    std::cerr << "topc: error: invalid --pcallgraph format '"
              << callGraphFormat << "' (expected: dot|ascii)\n";
    std::exit(EXIT_FAILURE);
  }
  if (!cfgFormat.empty() && !isValidGraphFormat(cfgFormat)) {
    std::cerr << "topc: error: invalid --pcfg format '" << cfgFormat
              << "' (expected: dot|ascii)\n";
    std::exit(EXIT_FAILURE);
  }

  const bool hasInspectionRequest =
      wantsSource || wantsAst || wantsSymbols || wantsTypes || wantsCallGraph ||
      wantsCFG || wantsOwnership || wantsBorrow;

  if (wantsConstraint && !hasInspectionRequest) {
    std::cerr << "topc: error: --constraint requires one of --ptype, --pcallgraph, "
                 "--pownership, or --pborrow\n";
    std::exit(EXIT_FAILURE);
  }

  const bool hasConstraintCapableView =
      wantsTypes || wantsCallGraph || wantsOwnership || wantsBorrow;
  if (wantsConstraint && !hasConstraintCapableView) {
    std::vector<std::string> unsupported;
    if (wantsSource) {
      unsupported.push_back("--psource");
    }
    if (wantsAst) {
      unsupported.push_back("--past");
    }
    if (wantsSymbols) {
      unsupported.push_back("--psym");
    }
    if (wantsCFG) {
      unsupported.push_back("--pcfg");
    }

    std::cerr << "topc: error: --constraint is unsupported for selected view";
    if (unsupported.size() > 1) {
      std::cerr << "s";
    }
    std::cerr << ": ";
    for (std::size_t i = 0; i < unsupported.size(); ++i) {
      if (i > 0) {
        std::cerr << ", ";
      }
      std::cerr << unsupported[i];
    }
    std::cerr << ". supported views: --ptype, --pcallgraph, --pownership, "
                 "--pborrow\n";
    std::exit(EXIT_FAILURE);
  }

  const bool compileFlagsRequested =
      outputfile.getNumOccurrences() > 0 || emitHrAsm.getNumOccurrences() > 0 ||
      disopt.getNumOccurrences() > 0 || emitSan.getNumOccurrences() > 0;
  const bool compileRequested = !hasInspectionRequest || compileFlagsRequested;

  loguru::g_preamble = false;
  bool logging = !logfile.getValue().empty();
  if (debug || logging) {
    loguru::g_preamble = true;
    loguru::g_preamble_date = false;
    loguru::g_preamble_time = false;
    loguru::g_preamble_uptime = false;
    loguru::g_preamble_thread = false;
    loguru::init(argc, argv);
    loguru::g_stderr_verbosity = logging ? loguru::Verbosity_ERROR : debug;
    if (logging) {
      loguru::add_file(logfile.getValue().c_str(), loguru::Append,
                       loguru::Verbosity_MAX);
    }
  }

  std::ifstream stream;
  stream.open(sourceFile);
  if (!stream.good()) {
    LOG_S(ERROR) << "topc: error: no such file: '" << sourceFile << "'";
    std::exit(EXIT_FAILURE);
  }

  /*
   * Program representations, e.g., ast, analysis results, etc., are
   * represented using shared pointers.  The driver "owns" this data and
   * it permits other components to read the contents by passing
   * the underlying pointer, i.e., via a call to get().
   */
  try {
    std::shared_ptr<ASTProgram> ast = FrontEnd::parse(stream);

    try {
      std::shared_ptr<SymbolTable> symTable;
      std::shared_ptr<IntraproceduralCFGs> cfgs;
      std::shared_ptr<CallGraph> callGraph;
      std::shared_ptr<TypeInference> typeResults;
      std::shared_ptr<OwnershipClassifier> ownershipClassifier;
      std::shared_ptr<FunctionEffectSummaries> functionEffectSummaries;

      bool structuralChecked = false;
      bool borrowChecked = false;
      bool interproceduralBorrowChecked = false;
      bool ownershipChecked = false;

      auto ensureSymbols = [&]() {
        if (!symTable) {
          symTable = SymbolTable::build(ast.get());
        }
      };

      auto ensureStructuralChecks = [&]() {
        if (structuralChecked) {
          return;
        }
        CheckAssignable::check(ast.get());
        CheckBorrowPositions::check(ast.get());
        CheckSumTypeNames::check(ast.get());
        CheckCaseCompleteness::check(ast.get());
        CheckPatternTypes::check(ast.get());
        structuralChecked = true;
      };

      auto ensureBorrowChecker = [&]() {
        if (borrowChecked) {
          return;
        }
        ensureStructuralChecks();
        BorrowChecker::check(ast.get());
        borrowChecked = true;
      };

      auto ensureCFGs = [&]() {
        if (cfgs) {
          return;
        }
        ensureSymbols();
        ensureBorrowChecker();
        cfgs = IntraproceduralCFGs::build(ast.get());
      };

      auto ensureCallGraphResult = [&]() {
        if (callGraph) {
          return;
        }
        ensureSymbols();
        ensureCFGs();
        callGraph = CallGraph::build(ast.get(), symTable.get());
      };

      auto ensureTypeResult = [&]() {
        if (typeResults) {
          return;
        }
        ensureCallGraphResult();
        typeResults = TypeInference::run(ast.get(), callGraph.get(),
                                         symTable.get());
        CheckAllocPayload::check(ast.get(), typeResults.get());
      };

      auto ensureFunctionEffects = [&]() {
        if (functionEffectSummaries) {
          return;
        }
        ensureTypeResult();
        ownershipClassifier = std::make_shared<OwnershipClassifier>(
            symTable.get(), typeResults.get());
        functionEffectSummaries = FunctionEffectSummaries::build(
            ast.get(), symTable.get(), typeResults.get(),
            ownershipClassifier.get());
      };

      auto ensureInterproceduralBorrowChecker = [&]() {
        if (interproceduralBorrowChecked) {
          return;
        }
        ensureFunctionEffects();
        BorrowChecker::checkInterprocedural(ast.get(), symTable.get(),
                                             functionEffectSummaries.get());
        borrowChecked = true;
        interproceduralBorrowChecked = true;
      };

      auto ensureOwnershipResult = [&]() {
        if (ownershipChecked) {
          return;
        }
        ensureInterproceduralBorrowChecker();
        MoveAnalysis(ast.get(), symTable.get(), ownershipClassifier.get(),
                     functionEffectSummaries.get());
        DestructionPass::run(ast.get(), symTable.get(), ownershipClassifier.get(),
                             functionEffectSummaries.get());
        ownershipChecked = true;
      };

      if (wantsSource) {
        FrontEnd::prettyprint(ast.get(), std::cout);
      }

      if (wantsAst) {
        const std::string suffix = astFormat == "ascii" ? ".ast.txt" : ".ast.dot";
        const auto path = resolveGraphPath(sourceFile, suffix, outputDir);
        std::ofstream astStream(path);
        if (!astStream.good()) {
          LOG_S(ERROR) << "topc: error: failed to open '" << path
                       << "' for writing";
          std::exit(EXIT_FAILURE);
        }
        FrontEnd::astVisualize(ast, astStream, astFormat);
      }

      if (wantsSymbols) {
        ensureSymbols();
        ensureStructuralChecks();
        ensureCFGs();
        symTable->print(std::cout);
      }

      if (wantsTypes) {
        ensureTypeResult();
        typeResults->print(std::cout);
        if (wantsConstraint) {
          printTypeConstraints(ast.get(), symTable.get(), typeResults.get(),
                               callGraph.get(), std::cout);
        }
      }

      if (wantsCallGraph) {
        ensureCallGraphResult();
        const std::string suffix =
            callGraphFormat == "ascii" ? ".callgraph.txt" : ".callgraph.dot";
        const auto path = resolveGraphPath(sourceFile, suffix, outputDir);
        std::ofstream cgStream(path);
        if (!cgStream.good()) {
          LOG_S(ERROR) << "topc: error: failed to open '" << path
                       << "' for writing";
          std::exit(EXIT_FAILURE);
        }

        if (callGraphFormat == "ascii") {
          writeCallGraphAscii(ast.get(), callGraph.get(), cgStream);
        } else {
          callGraph->print(cgStream);
        }

        if (wantsConstraint) {
          printCallGraphConstraints(ast.get(), callGraph.get(), std::cout);
        }
      }

      if (wantsCFG) {
        ensureCFGs();
        if (cfgFormat == "ascii") {
          for (const auto *cfg : cfgs->getAll()) {
            CFGRenderer::renderAscii(*cfg, std::cout);
          }
        } else {
          for (const auto *cfg : cfgs->getAll()) {
            const auto path = resolveGraphPath(
                sourceFile, "." + cfg->getFunction()->getName() + ".cfg.dot",
                outputDir);
            std::ofstream cfgStream(path);
            if (!cfgStream.good()) {
              LOG_S(ERROR) << "topc: error: failed to open '" << path
                           << "' for writing";
              std::exit(EXIT_FAILURE);
            }
            CFGRenderer::renderDot(*cfg, cfgStream);
          }
        }
      }

      if (wantsOwnership) {
        ensureOwnershipResult();
        printOwnershipResult(ast.get(), symTable.get(), ownershipClassifier.get(),
                             std::cout);
        if (wantsConstraint) {
          printOwnershipConstraints(symTable.get(), ownershipClassifier.get(),
                                    std::cout);
        }
      }

      if (wantsBorrow) {
        ensureInterproceduralBorrowChecker();
        printBorrowResult(std::cout);
        if (wantsConstraint) {
          printBorrowConstraints(ast.get(), std::cout);
        }
      }

      if (compileRequested) {
        ensureOwnershipResult();
        auto analysisResults = std::make_shared<SemanticAnalysis>(
          symTable, cfgs, typeResults, callGraph, ownershipClassifier,
          functionEffectSummaries);

        auto llvmModule = CodeGenerator::generate(ast.get(), analysisResults.get(),
                                                  sourceFile);

        if (!disopt) {
          Optimizer::optimize(llvmModule.get(), emitSan);
        }

        if (emitHrAsm) {
          CodeGenerator::emitHumanReadableAssembly(llvmModule.get(), outputfile);
        } else {
          CodeGenerator::emit(llvmModule.get(), outputfile);
        }
      }

    } catch (SemanticError &e) {
      LOG_S(ERROR) << "topc: " << e.what();
      LOG_S(ERROR) << "topc: semantic error";
      std::exit(EXIT_FAILURE);
    } catch (InternalError &e) { // LCOV_EXCL_LINE
      /* Internal errors should never happen, but we have logic to catch
       * them just in case.  We do not want to count these lines toward
       * coverage goals since a working compiler will never cover these.
       */
      LOG_S(ERROR) << "topc: " << e.what();	// LCOV_EXCL_LINE
      LOG_S(ERROR) << "topc: internal error";	// LCOV_EXCL_LINE
      std::exit(EXIT_FAILURE);			// LCOV_EXCL_LINE
    }
  } catch (ParseError &e) {
    LOG_S(ERROR) << "topc: " << e.what();	// LCOV_EXCL_LINE
    LOG_S(ERROR) << "topc: parse error";	// LCOV_EXCL_LINE
    std::exit(EXIT_FAILURE);			// LCOV_EXCL_LINE
  } // LCOV_EXCL_LINE
} // LCOV_EXCL_LINE

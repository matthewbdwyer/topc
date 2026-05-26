#include "ASTHelper.h"
#include "OwnershipClassifier.h"
#include "SemanticAnalysis.h"
#include "SymbolTable.h"
#include "TopAlpha.h"
#include "TopBorrowRef.h"
#include "TopFunction.h"
#include "TopInt.h"
#include "TopOwningRef.h"
#include "TopRef.h"
#include "TopSumType.h"
#include "TopVar.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <sstream>

// ---------------------------------------------------------------------------
// Unit tests: classifyType for each type term in the Phase 8 table
// ---------------------------------------------------------------------------

TEST_CASE("OwnershipClassifier: TopInt is Copy", "[OwnershipClassifier]") {
  TopInt t;
  REQUIRE(OwnershipClassifier::classifyType(&t) == OwnershipClass::Copy);
}

TEST_CASE("OwnershipClassifier: TopFunction is Copy", "[OwnershipClassifier]") {
  auto intT = std::make_shared<TopInt>();
  TopFunction f({intT}, intT);
  REQUIRE(OwnershipClassifier::classifyType(&f) == OwnershipClass::Copy);
}

TEST_CASE("OwnershipClassifier: TopOwningRef is Own", "[OwnershipClassifier]") {
  auto intT = std::make_shared<TopInt>();
  TopOwningRef ownRef(intT);
  REQUIRE(OwnershipClassifier::classifyType(&ownRef) == OwnershipClass::Own);
}

TEST_CASE("OwnershipClassifier: TopBorrowRef is Copy",
          "[OwnershipClassifier]") {
  auto intT = std::make_shared<TopInt>();
  TopBorrowRef borrowRef(intT);
  REQUIRE(OwnershipClassifier::classifyType(&borrowRef) ==
          OwnershipClass::Copy);
}

TEST_CASE("OwnershipClassifier: TopRef (legacy TIP) is Copy",
          "[OwnershipClassifier]") {
  auto intT = std::make_shared<TopInt>();
  TopRef ref(intT);
  REQUIRE(OwnershipClassifier::classifyType(&ref) == OwnershipClass::Copy);
}

TEST_CASE("OwnershipClassifier: TopSumType all-Copy payloads is Copy",
          "[OwnershipClassifier]") {
  auto intT = std::make_shared<TopInt>();
  // Option type with two ctors: None() and Some(int)
  TopSumType sum("Option", {"None", "Some"}, {intT},
                 {{"None", 0}, {"Some", 1}});
  REQUIRE(OwnershipClassifier::classifyType(&sum) == OwnershipClass::Copy);
}

TEST_CASE("OwnershipClassifier: TopSumType with Own payload is Own",
          "[OwnershipClassifier]") {
  auto intT = std::make_shared<TopInt>();
  auto ownRef = std::make_shared<TopOwningRef>(intT);
  // Box type: Wrapped(⭡int)
  TopSumType sum("Box", {"Wrapped"}, {ownRef}, {{"Wrapped", 1}});
  REQUIRE(OwnershipClassifier::classifyType(&sum) == OwnershipClass::Own);
}

TEST_CASE("OwnershipClassifier: TopVar / TopAlpha is Copy",
          "[OwnershipClassifier]") {
  // TopAlpha extends TopVar; unresolved type var -> Copy
  // TopAlpha(ASTNode*) accepts nullptr as a placeholder node.
  TopAlpha alpha(nullptr);
  REQUIRE(OwnershipClassifier::classifyType(&alpha) == OwnershipClass::Copy);
}

// ---------------------------------------------------------------------------
// Integration test: run classifier on a program with int, owning, and borrow
// variables; snapshot the classifications.
// ---------------------------------------------------------------------------

TEST_CASE("OwnershipClassifier: integration — int is Copy, alloc is Own, "
          "borrow is Copy",
          "[OwnershipClassifier]") {
  // Declare a sum type so isTopProgram=true, enabling TopOwningRef for alloc.
  // Note: q = &x is now illegal (Phase 10 BorrowChecker); borrow → Copy is
  // covered by the TopBorrowRef unit test above.
  std::stringstream program;
  program << R"(
    type Flag = On | Off;
    main() {
      var x, p;
      x = 42;
      p = alloc x;
      return 0;
    }
  )";

  auto ast = ASTHelper::build_ast(program);
  auto sa = SemanticAnalysis::analyze(ast.get());
  auto *sym = sa->getSymbolTable();
  auto *classifier = sa->getOwnershipClassifier();

  auto *mainDecl = sym->getFunction("main");
  REQUIRE(mainDecl != nullptr);

  auto *xDecl = sym->getLocal("x", mainDecl);
  auto *pDecl = sym->getLocal("p", mainDecl);
  REQUIRE(xDecl != nullptr);
  REQUIRE(pDecl != nullptr);

  // x = 42 → int → Copy
  REQUIRE(classifier->classify(xDecl) == OwnershipClass::Copy);
  // p = alloc x → ⭡int → Own
  REQUIRE(classifier->classify(pDecl) == OwnershipClass::Own);
}

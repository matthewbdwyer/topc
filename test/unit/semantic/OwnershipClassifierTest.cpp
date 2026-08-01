#include "ASTHelper.h"
#include "OwnershipClassifier.h"
#include "SemanticAnalysis.h"
#include "SymbolTable.h"
#include "TopAlpha.h"
#include "TopBorrowRef.h"
#include "TopFunction.h"
#include "TopInt.h"
#include "ReferenceMode.h"
#include "ReferenceType.h"
#include "TopOwningRef.h"
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

      TEST_CASE("OwnershipClassifier: concrete generic reference mode is honored",
            "[OwnershipClassifier][ReferenceMode]") {
        auto intType = std::make_shared<TopInt>();
        ReferenceType own(
          std::make_shared<ReferenceMode>(ReferenceMode::Mode::Own), intType);
        ReferenceType borrow(
          std::make_shared<ReferenceMode>(ReferenceMode::Mode::Borrow), intType);

        REQUIRE(OwnershipClassifier::classifyType(&own) == OwnershipClass::Own);
        REQUIRE(OwnershipClassifier::classifyType(&borrow) == OwnershipClass::Copy);
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
  // Borrow classification is covered by the TopBorrowRef unit test above.
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

TEST_CASE("OwnershipClassifier: polymorphic identity preserves owning mode",
          "[OwnershipClassifier][ReferenceMode]") {
  std::stringstream program;
  program << R"(
    identity(value) {
      return value;
    }
    main() {
      var first, second;
      first = alloc 42;
      second = identity(first);
      return *second;
    }
  )";

  auto ast = ASTHelper::build_ast(program);
  auto analysis = SemanticAnalysis::analyze(ast.get());
  auto *symbols = analysis->getSymbolTable();
  auto *mainDecl = symbols->getFunction("main");
  auto *firstDecl = symbols->getLocal("first", mainDecl);
  auto *secondDecl = symbols->getLocal("second", mainDecl);

  REQUIRE(std::dynamic_pointer_cast<TopOwningRef>(
              analysis->getTypeResults()->getInferredType(firstDecl)) !=
          nullptr);
  REQUIRE(std::dynamic_pointer_cast<TopOwningRef>(
              analysis->getTypeResults()->getInferredType(secondDecl)) !=
          nullptr);
  REQUIRE(analysis->getOwnershipClassifier()->classify(firstDecl) ==
          OwnershipClass::Own);
  REQUIRE(analysis->getOwnershipClassifier()->classify(secondDecl) ==
          OwnershipClass::Own);
}

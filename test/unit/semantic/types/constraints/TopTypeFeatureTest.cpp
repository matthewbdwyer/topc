#include "ASTHelper.h"
#include "SymbolTable.h"
#include "TopBorrowRef.h"
#include "TopFunction.h"
#include "TopInt.h"
#include "TopModeVar.h"
#include "TopOwningRef.h"
#include "ReferenceMode.h"
#include "ReferenceType.h"
#include "TopSumType.h"
#include "TopVar.h"
#include "TypeConstraintCollectVisitor.h"
#include "Unifier.h"

#include <catch2/catch_test_macros.hpp>

#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

/*
 * Run the front-end on the program, collect the type constraints, solve them,
 * and return (unifier, symbolTable).  Expects no type errors.
 */
static std::pair<Unifier, std::shared_ptr<SymbolTable>>
collectAndSolve(std::stringstream &program) {
  auto ast = ASTHelper::build_ast(program);
  auto symbols = SymbolTable::build(ast.get());

  TypeConstraintCollectVisitor visitor(symbols.get());
  ast->accept(&visitor);

  auto collected = visitor.getCollectedConstraints();
  Unifier unifier(collected);
  REQUIRE_NOTHROW(unifier.solve());

  return {unifier, symbols};
}

// ---------------------------------------------------------------------------
// TopOwningRef: alloc in a TOP program infers TopOwningRef
// ---------------------------------------------------------------------------
TEST_CASE("Type constraints: alloc infers TopOwningRef",
          "[TopTypeFeature][TypeConstraint][TopOwningRef]") {
  std::stringstream stream;
  stream << R"(
    type Color = Red | Blue;
    f() {
      var p;
      p = alloc 5;
      return 0;
    }
    main() { return 0; }
  )";

  auto [unifier, symbols] = collectAndSolve(stream);

  auto fDecl = symbols->getFunction("f");
  auto pDecl = symbols->getLocal("p", fDecl);
  REQUIRE(pDecl != nullptr);

  auto pVar = std::make_shared<TopVar>(pDecl);
  auto inferred = unifier.inferred(pVar);
  REQUIRE(dynamic_cast<TopOwningRef *>(inferred.get()) != nullptr);
  auto ownRef = dynamic_cast<TopOwningRef *>(inferred.get());
  REQUIRE(*ownRef->getReferencedType() == TopInt());
}

// ---------------------------------------------------------------------------
// TopBorrowRef: &x infers TopBorrowRef
// ---------------------------------------------------------------------------
TEST_CASE("Type constraints: address-of expression infers TopBorrowRef",
          "[TopTypeFeature][TypeConstraint][TopBorrowRef]") {
  std::stringstream stream;
  stream << R"(
    type Color = Red | Blue;
    f() {
      var x, r;
      x = 42;
      r = &x;
      return 0;
    }
    main() { return 0; }
  )";

  auto [unifier, symbols] = collectAndSolve(stream);

  auto fDecl = symbols->getFunction("f");
  auto xDecl = symbols->getLocal("x", fDecl);
  auto rDecl = symbols->getLocal("r", fDecl);
  REQUIRE(xDecl != nullptr);
  REQUIRE(rDecl != nullptr);

  auto rVar = std::make_shared<TopVar>(rDecl);
  auto inferred = unifier.inferred(rVar);
  REQUIRE(dynamic_cast<TopBorrowRef *>(inferred.get()) != nullptr);
  auto borrowRef = dynamic_cast<TopBorrowRef *>(inferred.get());
  REQUIRE(*borrowRef->getReferencedType() == TopInt());
}

TEST_CASE("Type constraints: borrowed reference can be dereferenced",
          "[TopTypeFeature][TypeConstraint][TopBorrowRef]") {
  std::stringstream stream;
  stream << R"(
    type Color = Red | Blue;
    read(b) {
      return *b;
    }
    main() {
      var x;
      x = 42;
      return read(&x);
    }
  )";

  auto [unifier, symbols] = collectAndSolve(stream);

  auto readDecl = symbols->getFunction("read");
  auto bDecl = symbols->getLocal("b", readDecl);
  REQUIRE(bDecl != nullptr);

  auto readType = std::dynamic_pointer_cast<TopFunction>(
      unifier.inferred(std::make_shared<TopVar>(readDecl)));
  REQUIRE(readType != nullptr);
  REQUIRE(readType->getParamTypes().size() == 1);

  auto borrowRef = std::dynamic_pointer_cast<TopBorrowRef>(
      readType->getParamTypes()[0]);
  REQUIRE(borrowRef != nullptr);
  REQUIRE(*borrowRef->getReferencedType() == TopInt());
  REQUIRE(*readType->getReturnType() == TopInt());

  auto bRef = std::dynamic_pointer_cast<ReferenceType>(
      unifier.inferred(std::make_shared<TopVar>(bDecl)));
  REQUIRE(bRef != nullptr);
  auto mode = std::dynamic_pointer_cast<ReferenceMode>(bRef->getMode());
  REQUIRE(mode != nullptr);
  REQUIRE(mode->getMode() == ReferenceMode::Mode::Borrow);
  REQUIRE(*bRef->getReferencedType() == TopInt());
}

// ---------------------------------------------------------------------------
// TopSumType: case expression is inferred as TopSumType
// ---------------------------------------------------------------------------
TEST_CASE("Type constraints: case expression inferred as TopSumType",
          "[TopTypeFeature][TypeConstraint][TopSumType]") {
  std::stringstream stream;
  stream << R"(
    type Option = Some(x) | None;
    main() {
      var o;
      case o of { Some(v) -> output v; None -> output 0; }
      return 0;
    }
  )";

  auto [unifier, symbols] = collectAndSolve(stream);

  auto mainDecl = symbols->getFunction("main");
  auto oDecl = symbols->getLocal("o", mainDecl);
  REQUIRE(oDecl != nullptr);

  auto oVar = std::make_shared<TopVar>(oDecl);
  auto inferred = unifier.inferred(oVar);
  REQUIRE(dynamic_cast<TopSumType *>(inferred.get()) != nullptr);

  auto sumTy = dynamic_cast<TopSumType *>(inferred.get());
  REQUIRE(sumTy->getTypeName() == "Option");
}

// ---------------------------------------------------------------------------
// TopSumType: case arm binding type equals the constructor payload type
// ---------------------------------------------------------------------------
TEST_CASE("Type constraints: case arm binding matches constructor payload",
          "[TopTypeFeature][TypeConstraint][TopSumType]") {
  std::stringstream stream;
  stream << R"(
    type Wrap = Box(val);
    main() {
      var w;
      case w of { Box(v) -> output v; }
      return 0;
    }
  )";

  auto [unifier, symbols] = collectAndSolve(stream);

  auto mainDecl = symbols->getFunction("main");
  // 'v' is the arm binding; output v forces [[v]] = int
  auto vDecl = symbols->getLocal("v", mainDecl);
  REQUIRE(vDecl != nullptr);

  auto vVar = std::make_shared<TopVar>(vDecl);
  auto inferred = unifier.inferred(vVar);
  REQUIRE(*inferred == TopInt());
}

// ---------------------------------------------------------------------------
// TopSumType equality and print
// ---------------------------------------------------------------------------
TEST_CASE("TopSumType: equality and print", "[TopTypeFeature][TopSumType]") {
  auto intTy = std::make_shared<TopInt>();
  std::vector<std::string> ctors = {"Some", "None"};
  std::vector<std::shared_ptr<TopType>> payloads = {intTy};
  std::map<std::string, int> arities = {{"Some", 1}, {"None", 0}};

  auto t1 = std::make_shared<TopSumType>("Option", ctors, payloads, arities);
  auto t2 = std::make_shared<TopSumType>("Option", ctors, payloads, arities);
  auto t3 = std::make_shared<TopSumType>("Result", ctors, payloads, arities);

  REQUIRE(*t1 == *t2);
  REQUIRE(!(*t1 == *t3));

  std::ostringstream oss;
  oss << *t1;
  REQUIRE(oss.str() == "Option{Some(int)|None}");
}

// ---------------------------------------------------------------------------
// TopOwningRef equality and print
// ---------------------------------------------------------------------------
TEST_CASE("TopOwningRef: equality and print",
          "[TopTypeFeature][TopOwningRef]") {
  auto intTy = std::make_shared<TopInt>();
  auto r1 = std::make_shared<TopOwningRef>(intTy);
  auto r2 = std::make_shared<TopOwningRef>(intTy);
  auto r3 = std::make_shared<TopBorrowRef>(intTy);

  REQUIRE(*r1 == *r2);
  REQUIRE(!(*r1 == *r3));

  std::ostringstream oss;
  oss << *r1;
  REQUIRE(oss.str() == "own&int");
}

// ---------------------------------------------------------------------------
// TopBorrowRef equality and print
// ---------------------------------------------------------------------------
TEST_CASE("TopBorrowRef: equality and print",
          "[TopTypeFeature][TopBorrowRef]") {
  auto intTy = std::make_shared<TopInt>();
  auto r1 = std::make_shared<TopBorrowRef>(intTy);
  auto r2 = std::make_shared<TopBorrowRef>(intTy);

  REQUIRE(*r1 == *r2);

  std::ostringstream oss;
  oss << *r1;
  REQUIRE(oss.str() == "borrow&int");
}

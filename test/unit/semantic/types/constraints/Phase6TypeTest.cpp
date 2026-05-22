#include "ASTHelper.h"
#include "SymbolTable.h"
#include "TipBorrowRef.h"
#include "TipInt.h"
#include "TipOwningRef.h"
#include "TipSumType.h"
#include "TipVar.h"
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
// TipOwningRef: alloc in a TOP program infers TipOwningRef
// ---------------------------------------------------------------------------
TEST_CASE("Phase6: alloc in TOP program infers TipOwningRef",
          "[Phase6][TypeConstraint]") {
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

  auto pVar = std::make_shared<TipVar>(pDecl);
  auto inferred = unifier.inferred(pVar);
  REQUIRE(dynamic_cast<TipOwningRef *>(inferred.get()) != nullptr);
  auto ownRef = dynamic_cast<TipOwningRef *>(inferred.get());
  REQUIRE(*ownRef->getReferencedType() == TipInt());
}

// ---------------------------------------------------------------------------
// TipBorrowRef: &x in a TOP program infers TipBorrowRef
// ---------------------------------------------------------------------------
TEST_CASE("Phase6: borrow expression in TOP program infers TipBorrowRef",
          "[Phase6][TypeConstraint]") {
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

  auto rVar = std::make_shared<TipVar>(rDecl);
  auto inferred = unifier.inferred(rVar);
  REQUIRE(dynamic_cast<TipBorrowRef *>(inferred.get()) != nullptr);
  auto borrowRef = dynamic_cast<TipBorrowRef *>(inferred.get());
  REQUIRE(*borrowRef->getReferencedType() == TipInt());
}

// ---------------------------------------------------------------------------
// TipSumType: case scrutinee is inferred as TipSumType
// ---------------------------------------------------------------------------
TEST_CASE("Phase6: case scrutinee inferred as TipSumType",
          "[Phase6][TypeConstraint]") {
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

  auto oVar = std::make_shared<TipVar>(oDecl);
  auto inferred = unifier.inferred(oVar);
  REQUIRE(dynamic_cast<TipSumType *>(inferred.get()) != nullptr);

  auto sumTy = dynamic_cast<TipSumType *>(inferred.get());
  REQUIRE(sumTy->getTypeName() == "Option");
}

// ---------------------------------------------------------------------------
// TipSumType: case arm binding type equals the constructor payload type
// ---------------------------------------------------------------------------
TEST_CASE("Phase6: case arm binding has same type as constructor payload",
          "[Phase6][TypeConstraint]") {
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

  auto vVar = std::make_shared<TipVar>(vDecl);
  auto inferred = unifier.inferred(vVar);
  REQUIRE(*inferred == TipInt());
}

// ---------------------------------------------------------------------------
// TipSumType equality and print
// ---------------------------------------------------------------------------
TEST_CASE("Phase6: TipSumType equality and print", "[Phase6][TipSumType]") {
  auto intTy = std::make_shared<TipInt>();
  std::vector<std::string> ctors = {"Some", "None"};
  std::vector<std::shared_ptr<TipType>> payloads = {intTy};
  std::map<std::string, int> arities = {{"Some", 1}, {"None", 0}};

  auto t1 = std::make_shared<TipSumType>("Option", ctors, payloads, arities);
  auto t2 = std::make_shared<TipSumType>("Option", ctors, payloads, arities);
  auto t3 = std::make_shared<TipSumType>("Result", ctors, payloads, arities);

  REQUIRE(*t1 == *t2);
  REQUIRE(!(*t1 == *t3));

  std::ostringstream oss;
  oss << *t1;
  REQUIRE(oss.str() == "Option{Some(int)|None}");
}

// ---------------------------------------------------------------------------
// TipOwningRef equality and print
// ---------------------------------------------------------------------------
TEST_CASE("Phase6: TipOwningRef equality and print",
          "[Phase6][TipOwningRef]") {
  auto intTy = std::make_shared<TipInt>();
  auto r1 = std::make_shared<TipOwningRef>(intTy);
  auto r2 = std::make_shared<TipOwningRef>(intTy);
  auto r3 = std::make_shared<TipBorrowRef>(intTy);

  REQUIRE(*r1 == *r2);
  REQUIRE(!(*r1 == *r3));

  std::ostringstream oss;
  oss << *r1;
  REQUIRE(oss.str() == "own&int");
}

// ---------------------------------------------------------------------------
// TipBorrowRef equality and print
// ---------------------------------------------------------------------------
TEST_CASE("Phase6: TipBorrowRef equality and print",
          "[Phase6][TipBorrowRef]") {
  auto intTy = std::make_shared<TipInt>();
  auto r1 = std::make_shared<TipBorrowRef>(intTy);
  auto r2 = std::make_shared<TipBorrowRef>(intTy);

  REQUIRE(*r1 == *r2);

  std::ostringstream oss;
  oss << *r1;
  REQUIRE(oss.str() == "borrow&int");
}

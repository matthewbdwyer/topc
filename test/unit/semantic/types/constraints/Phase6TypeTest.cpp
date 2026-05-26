#include "ASTHelper.h"
#include "SymbolTable.h"
#include "TopBorrowRef.h"
#include "TopInt.h"
#include "TopOwningRef.h"
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
TEST_CASE("Phase6: alloc in TOP program infers TopOwningRef",
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

  auto pVar = std::make_shared<TopVar>(pDecl);
  auto inferred = unifier.inferred(pVar);
  REQUIRE(dynamic_cast<TopOwningRef *>(inferred.get()) != nullptr);
  auto ownRef = dynamic_cast<TopOwningRef *>(inferred.get());
  REQUIRE(*ownRef->getReferencedType() == TopInt());
}

// ---------------------------------------------------------------------------
// TopBorrowRef: &x in a TOP program infers TopBorrowRef
// ---------------------------------------------------------------------------
TEST_CASE("Phase6: borrow expression in TOP program infers TopBorrowRef",
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

  auto rVar = std::make_shared<TopVar>(rDecl);
  auto inferred = unifier.inferred(rVar);
  REQUIRE(dynamic_cast<TopBorrowRef *>(inferred.get()) != nullptr);
  auto borrowRef = dynamic_cast<TopBorrowRef *>(inferred.get());
  REQUIRE(*borrowRef->getReferencedType() == TopInt());
}

// ---------------------------------------------------------------------------
// TopSumType: case scrutinee is inferred as TopSumType
// ---------------------------------------------------------------------------
TEST_CASE("Phase6: case scrutinee inferred as TopSumType",
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

  auto oVar = std::make_shared<TopVar>(oDecl);
  auto inferred = unifier.inferred(oVar);
  REQUIRE(dynamic_cast<TopSumType *>(inferred.get()) != nullptr);

  auto sumTy = dynamic_cast<TopSumType *>(inferred.get());
  REQUIRE(sumTy->getTypeName() == "Option");
}

// ---------------------------------------------------------------------------
// TopSumType: case arm binding type equals the constructor payload type
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

  auto vVar = std::make_shared<TopVar>(vDecl);
  auto inferred = unifier.inferred(vVar);
  REQUIRE(*inferred == TopInt());
}

// ---------------------------------------------------------------------------
// TopSumType equality and print
// ---------------------------------------------------------------------------
TEST_CASE("Phase6: TopSumType equality and print", "[Phase6][TopSumType]") {
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
TEST_CASE("Phase6: TopOwningRef equality and print",
          "[Phase6][TopOwningRef]") {
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
TEST_CASE("Phase6: TopBorrowRef equality and print",
          "[Phase6][TopBorrowRef]") {
  auto intTy = std::make_shared<TopInt>();
  auto r1 = std::make_shared<TopBorrowRef>(intTy);
  auto r2 = std::make_shared<TopBorrowRef>(intTy);

  REQUIRE(*r1 == *r2);

  std::ostringstream oss;
  oss << *r1;
  REQUIRE(oss.str() == "borrow&int");
}

#include "TipTermBridge.h"
#include "ASTHelper.h"
#include "ASTVariableExpr.h"
#include "TipAlpha.h"
#include "TipFunction.h"
#include "TipInt.h"
#include "TipMu.h"
#include "TipRef.h"
#include "TypeConstraintCollectVisitor.h"
#include "TypeConstraintUnifyVisitor.h"
#include "TypeConstraintVisitor.h"
#include "UnificationError.h"

#include <catch2/catch_test_macros.hpp>

#include <iostream>

TEST_CASE("TipTermBridge: Collect and then unify constraints",
          "[TipTermBridge, Collect]") {

  SECTION("Test type-safe program 1") {
    std::stringstream program;
    program << R"(
            // x is int, y is &int, z is int, short is () -> int
            short() {
              var x, y, z;
              x = input;	
              y = alloc x;
              *y = x;
              z = *y;
              return z;
            }
         )";

    auto ast = ASTHelper::build_ast(program);
    auto symbols = SymbolTable::build(ast.get());

    TypeConstraintCollectVisitor visitor(symbols.get());
    ast->accept(&visitor);

    TipTermBridge bridge(visitor.getCollectedConstraints());
    REQUIRE_NOTHROW(bridge.solve());

    // Expected types
    std::vector<std::shared_ptr<TipType>> emptyParams;
    auto intType = std::make_shared<TipInt>();
    auto funRetInt = std::make_shared<TipFunction>(emptyParams, intType);
    auto ptrToInt = std::make_shared<TipRef>(intType);

    auto fDecl = symbols->getFunction("short");
    auto fType = std::make_shared<TipVar>(fDecl);

    REQUIRE(*bridge.inferred(fType) == *funRetInt);

    auto xType = std::make_shared<TipVar>(symbols->getLocal("x", fDecl));
    REQUIRE(*bridge.inferred(xType) == *intType);

    auto yType = std::make_shared<TipVar>(symbols->getLocal("y", fDecl));
    REQUIRE(*bridge.inferred(yType) == *ptrToInt);

    auto zType = std::make_shared<TipVar>(symbols->getLocal("z", fDecl));
    REQUIRE(*bridge.inferred(zType) == *intType);
  }

  SECTION("Test type-safe deref") {
    std::stringstream program;
    program << R"(
// deref is (&\alpha<*p>) -> \alpha<*p>, p is &\alpha<*p>
deref(p){
    return *p;
}
         )";

    auto ast = ASTHelper::build_ast(program);
    auto symbols = SymbolTable::build(ast.get());

    TypeConstraintCollectVisitor visitor(symbols.get());
    ast->accept(&visitor);

    TipTermBridge bridge(visitor.getCollectedConstraints());
    REQUIRE_NOTHROW(bridge.solve());

    auto fDecl = symbols->getFunction("deref");
    auto fType = std::make_shared<TipVar>(fDecl);
    auto pType = std::make_shared<TipVar>(symbols->getLocal("p", fDecl));

    auto polyInferred = bridge.inferred(fType);
    auto polyFun = std::dynamic_pointer_cast<TipFunction>(polyInferred);
    REQUIRE(polyFun != nullptr);
    REQUIRE(polyFun->getParamTypes().size() == 1);
    auto polyArg = polyFun->getParamTypes().back();
    auto polyArgRef = std::dynamic_pointer_cast<TipRef>(polyArg);
    REQUIRE(polyArgRef != nullptr);
    auto polyArgAddressOfField = polyArgRef->getReferencedType();
    REQUIRE(std::dynamic_pointer_cast<TipAlpha>(polyArgAddressOfField));

    auto pInferred = bridge.inferred(pType);
    REQUIRE(*pInferred == *polyArg);
  }

  SECTION("Test unification error 1") {
    std::stringstream program;
    program << R"(
            bar(g,x) {
                var r;
                if (x==0){
                    r=g;
                } else {
                    r=bar(2,0);
                }
                return r+1;
            }

            main() {
                return bar(null,1);
            }
        )";

    auto ast = ASTHelper::build_ast(program);
    auto symbols = SymbolTable::build(ast.get());

    TypeConstraintCollectVisitor visitor(symbols.get());
    ast->accept(&visitor);

    TipTermBridge bridge(visitor.getCollectedConstraints());
    REQUIRE_THROWS_AS(bridge.solve(), UnificationError);
  }

  SECTION("Test unification error 2") {
    std::stringstream program;
    program << R"(
            foo(p) {
                return *p;
            }

            main() {
                var x;
                x = 5;
                x = foo;
                return 4;
            }
        )";

    auto ast = ASTHelper::build_ast(program);
    auto symbols = SymbolTable::build(ast.get());

    TypeConstraintCollectVisitor visitor(symbols.get());
    ast->accept(&visitor);

    TipTermBridge bridge(visitor.getCollectedConstraints());
    REQUIRE_THROWS_AS(bridge.solve(), UnificationError);
  }

  SECTION("Test unification error 3") {
    std::stringstream program;
    program << R"(
            main() {
                var x, y;
                x = 5;
                y = 10;
                x = &y;
                return 4;
            }
        )";

    auto ast = ASTHelper::build_ast(program);
    auto symbols = SymbolTable::build(ast.get());

    TypeConstraintCollectVisitor visitor(symbols.get());
    ast->accept(&visitor);

    TipTermBridge bridge(visitor.getCollectedConstraints());
    REQUIRE_THROWS_AS(bridge.solve(), UnificationError);
  }
}

TEST_CASE("TipTermBridge: Unify constraints on the fly",
          "[TipTermBridge, On-the-fly]") {

  SECTION("Test type-safe program 1") {
    std::stringstream program;
    program << R"(
            short() {
              var x, y, z;
              x = input;
              y = alloc x;
              *y = x;
              z = *y;
              return z;
            }
         )";

    auto ast = ASTHelper::build_ast(program);
    auto symbols = SymbolTable::build(ast.get());

    TypeConstraintUnifyVisitor visitor(symbols.get());
    REQUIRE_NOTHROW(ast->accept(&visitor));
  }

  SECTION("Test type-safe record2") {
    std::stringstream program;
    program << R"(
main() {
    var n, r1;
    n = alloc {p: 4, q: 2};
    *n = {p:5, q: 6};
    r1 = (*n).p; // output 5
    output r1;
    return 0;
}    
         )";

    auto ast = ASTHelper::build_ast(program);
    auto symbols = SymbolTable::build(ast.get());

    TypeConstraintUnifyVisitor visitor(symbols.get());
    REQUIRE_NOTHROW(ast->accept(&visitor));
  }

  SECTION("Test type-safe record4") {
    std::stringstream program;
    program << R"(
main() {
    var n, k, r1;
    k = {a: 1, b: 2};
    n = {c: &k, d: 4};
    r1 = ((*(n.c)).a); // output 1
    output r1;
    return 0;
}
         )";

    auto ast = ASTHelper::build_ast(program);
    auto symbols = SymbolTable::build(ast.get());

    TypeConstraintUnifyVisitor visitor(symbols.get());
    REQUIRE_NOTHROW(ast->accept(&visitor));
  }

  SECTION("Test type-safe deref") {
    std::stringstream program;
    program << R"(
deref(p){
    return *p;
}
         )";

    auto ast = ASTHelper::build_ast(program);
    auto symbols = SymbolTable::build(ast.get());

    TypeConstraintUnifyVisitor visitor(symbols.get());
    REQUIRE_NOTHROW(ast->accept(&visitor));
  }

  SECTION("Test unification error 1") {
    std::stringstream program;
    program << R"(
            bar(g,x) {
                var r;
                if (x==0){
                    r=g;
                } else {
                    r=bar(2,0);
                }
                return r+1;
            }

            main() {
                return bar(null,1);
            }
        )";

    auto ast = ASTHelper::build_ast(program);
    auto symbols = SymbolTable::build(ast.get());

    TypeConstraintUnifyVisitor visitor(symbols.get());
    REQUIRE_THROWS_AS(ast->accept(&visitor), UnificationError);
  }

  SECTION("Test unification error 2") {
    std::stringstream program;
    program << R"(
            foo(p) {
                return *p;
            }

            main() {
                var x;
                x = 5;
                x = foo;
                return 4;
            }
        )";

    auto ast = ASTHelper::build_ast(program);
    auto symbols = SymbolTable::build(ast.get());

    TypeConstraintUnifyVisitor visitor(symbols.get());
    REQUIRE_THROWS_AS(ast->accept(&visitor), UnificationError);
  }

  SECTION("Test unification error 3") {
    std::stringstream program;
    program << R"(
            main() {
                var x, y;
                x = 5;
                y = 10;
                x = &y;
                return 4;
            }
        )";

    auto ast = ASTHelper::build_ast(program);
    auto symbols = SymbolTable::build(ast.get());

    TypeConstraintUnifyVisitor visitor(symbols.get());
    REQUIRE_THROWS_AS(ast->accept(&visitor), UnificationError);
  }

  SECTION("Test unification error 4") {
    std::stringstream program;
    program << R"(
        foo() {
            var r, q;
            q = {f: 1, h: 3};
            r = {f: 4, g: 13};
            r.h = q.h;
            return r.g;
        }
        )";
    auto ast = ASTHelper::build_ast(program);
    auto symbols = SymbolTable::build(ast.get());

    TypeConstraintUnifyVisitor visitor(symbols.get());
    REQUIRE_THROWS_AS(ast->accept(&visitor), UnificationError);
  }
}

TEST_CASE("TipTermBridge: Test unifying TipCons with different arities",
          "[TipTermBridge]") {
  std::vector<std::shared_ptr<TipType>> paramsA{std::make_shared<TipInt>()};
  auto retA = std::make_shared<TipInt>();
  auto tipFunctionA = std::make_shared<TipFunction>(paramsA, retA);

  std::vector<std::shared_ptr<TipType>> paramsB{std::make_shared<TipInt>(),
                                                std::make_shared<TipInt>()};
  auto retB = std::make_shared<TipInt>();
  auto tipFunctionB = std::make_shared<TipFunction>(paramsB, retB);

  TypeConstraint constraint(tipFunctionA, tipFunctionB);
  std::vector<TypeConstraint> constraints{constraint};

  TipTermBridge bridge(constraints);
  REQUIRE_THROWS_AS(bridge.unify(tipFunctionA, tipFunctionB), UnificationError);
}

TEST_CASE("TipTermBridge: Test unifying TipCons with the same arity",
          "[TipTermBridge]") {
  std::vector<std::shared_ptr<TipType>> params{std::make_shared<TipInt>()};
  auto ret = std::make_shared<TipInt>();
  auto tipFunctionA = std::make_shared<TipFunction>(params, ret);

  auto tipFunctionB = std::make_shared<TipFunction>(params, ret);

  TypeConstraint constraint(tipFunctionA, tipFunctionB);
  std::vector<TypeConstraint> constraints{constraint};

  TipTermBridge bridge(constraints);
  REQUIRE_NOTHROW(bridge.unify(tipFunctionA, tipFunctionB));
}

TEST_CASE("TipTermBridge: Test unifying proper types with a type variable",
          "[TipTermBridge]") {
  ASTVariableExpr variableExpr("foo");
  auto tipVar = std::make_shared<TipVar>(&variableExpr);
  auto tipInt = std::make_shared<TipInt>();

  TypeConstraint constraint(tipVar, tipInt);
  std::vector<TypeConstraint> constraints{constraint};

  TipTermBridge bridge(constraints);
  REQUIRE_NOTHROW(bridge.unify(tipVar, tipInt));
}

TEST_CASE("TipTermBridge: Test unifying two different type variables",
          "[TipTermBridge]") {
  ASTVariableExpr variableExprA("foo");
  auto tipVarA = std::make_shared<TipVar>(&variableExprA);

  ASTVariableExpr variableExprB("foo");
  auto tipVarB = std::make_shared<TipVar>(&variableExprB);

  TypeConstraint constraint(tipVarA, tipVarB);
  std::vector<TypeConstraint> constraints{constraint};

  TipTermBridge bridge(constraints);
  REQUIRE_NOTHROW(bridge.unify(tipVarA, tipVarB));
}

TEST_CASE("TipTermBridge: Test closing mu", "[TipTermBridge]") {
  ASTVariableExpr variableExprG("g");
  auto theAlphaG = std::make_shared<TipAlpha>(&variableExprG);

  auto theInt = std::make_shared<TipInt>();

  ASTVariableExpr variableExprFoo("foo");
  auto theVarFoo = std::make_shared<TipVar>(&variableExprFoo);

  ASTVariableExpr variableExprF("f");
  auto theAlphaF = std::make_shared<TipAlpha>(&variableExprF);

  std::vector<std::shared_ptr<TipType>> params{theAlphaF, theAlphaG};
  auto theFunction = std::make_shared<TipFunction>(params, theAlphaG);

  auto theMu = std::make_shared<TipMu>(theAlphaF, theFunction);

  TypeConstraint constraint1(theAlphaG, theInt);
  TypeConstraint constraint2(theVarFoo, theMu);
  std::vector<TypeConstraint> constraints{constraint1, constraint2};
  TipTermBridge bridge(constraints);

  auto closed = bridge.inferred(theMu);

  std::stringstream ss;
  ss << *closed;

  REQUIRE_NOTHROW(ss.str() == "\u03bc\u03B1<f>.(\u03B1<f>,int) -> int");
}

TEST_CASE("TipTermBridge: static type predicates", "[TipTermBridge]") {
  ASTVariableExpr varExpr("x");

  REQUIRE(TipTermBridge::isVar(std::make_shared<TipVar>(&varExpr)));
  REQUIRE(TipTermBridge::isCons(std::make_shared<TipInt>()));
  REQUIRE(TipTermBridge::isMu(std::make_shared<TipMu>(
      std::make_shared<TipAlpha>(&varExpr),
      std::make_shared<TipInt>())));
  REQUIRE(TipTermBridge::isAlpha(std::make_shared<TipAlpha>(&varExpr)));
  REQUIRE(TipTermBridge::isProperType(std::make_shared<TipInt>()));
  REQUIRE_FALSE(TipTermBridge::isProperType(std::make_shared<TipVar>(&varExpr)));
}

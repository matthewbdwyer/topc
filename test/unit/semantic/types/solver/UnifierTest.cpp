#include "Unifier.h"
#include "ASTHelper.h"
#include "ASTVariableExpr.h"
#include "TopAlpha.h"
#include "TopFunction.h"
#include "TopInt.h"
#include "TopMu.h"
#include "TopRef.h"
#include "TypeConstraintCollectVisitor.h"
#include "TypeConstraintUnifyVisitor.h"
#include "TypeConstraintVisitor.h"
#include "UnificationError.h"

#include <catch2/catch_test_macros.hpp>

#include <iostream>

TEST_CASE("Unifier: Collect and then unify constraints",
          "[Unifier, Collect]") {

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

    Unifier bridge(visitor.getCollectedConstraints());
    REQUIRE_NOTHROW(bridge.solve());

    // Expected types
    std::vector<std::shared_ptr<TopType>> emptyParams;
    auto intType = std::make_shared<TopInt>();
    auto funRetInt = std::make_shared<TopFunction>(emptyParams, intType);
    auto ptrToInt = std::make_shared<TopRef>(intType);

    auto fDecl = symbols->getFunction("short");
    auto fType = std::make_shared<TopVar>(fDecl);

    REQUIRE(*bridge.inferred(fType) == *funRetInt);

    auto xType = std::make_shared<TopVar>(symbols->getLocal("x", fDecl));
    REQUIRE(*bridge.inferred(xType) == *intType);

    auto yType = std::make_shared<TopVar>(symbols->getLocal("y", fDecl));
    REQUIRE(*bridge.inferred(yType) == *ptrToInt);

    auto zType = std::make_shared<TopVar>(symbols->getLocal("z", fDecl));
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

    Unifier bridge(visitor.getCollectedConstraints());
    REQUIRE_NOTHROW(bridge.solve());

    auto fDecl = symbols->getFunction("deref");
    auto fType = std::make_shared<TopVar>(fDecl);
    auto pType = std::make_shared<TopVar>(symbols->getLocal("p", fDecl));

    auto polyInferred = bridge.inferred(fType);
    auto polyFun = std::dynamic_pointer_cast<TopFunction>(polyInferred);
    REQUIRE(polyFun != nullptr);
    REQUIRE(polyFun->getParamTypes().size() == 1);
    auto polyArg = polyFun->getParamTypes().back();
    auto polyArgRef = std::dynamic_pointer_cast<TopRef>(polyArg);
    REQUIRE(polyArgRef != nullptr);
    auto polyArgAddressOfField = polyArgRef->getReferencedType();
    REQUIRE(std::dynamic_pointer_cast<TopAlpha>(polyArgAddressOfField));

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

    Unifier bridge(visitor.getCollectedConstraints());
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

    Unifier bridge(visitor.getCollectedConstraints());
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

    Unifier bridge(visitor.getCollectedConstraints());
    REQUIRE_THROWS_AS(bridge.solve(), UnificationError);
  }
}

TEST_CASE("Unifier: Unify constraints on the fly",
          "[Unifier, On-the-fly]") {

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

TEST_CASE("Unifier: Test unifying TopCons with different arities",
          "[Unifier]") {
  std::vector<std::shared_ptr<TopType>> paramsA{std::make_shared<TopInt>()};
  auto retA = std::make_shared<TopInt>();
  auto tipFunctionA = std::make_shared<TopFunction>(paramsA, retA);

  std::vector<std::shared_ptr<TopType>> paramsB{std::make_shared<TopInt>(),
                                                std::make_shared<TopInt>()};
  auto retB = std::make_shared<TopInt>();
  auto tipFunctionB = std::make_shared<TopFunction>(paramsB, retB);

  TypeConstraint constraint(tipFunctionA, tipFunctionB);
  std::vector<TypeConstraint> constraints{constraint};

  Unifier bridge(constraints);
  REQUIRE_THROWS_AS(bridge.unify(tipFunctionA, tipFunctionB), UnificationError);
}

TEST_CASE("Unifier: Test unifying TopCons with the same arity",
          "[Unifier]") {
  std::vector<std::shared_ptr<TopType>> params{std::make_shared<TopInt>()};
  auto ret = std::make_shared<TopInt>();
  auto tipFunctionA = std::make_shared<TopFunction>(params, ret);

  auto tipFunctionB = std::make_shared<TopFunction>(params, ret);

  TypeConstraint constraint(tipFunctionA, tipFunctionB);
  std::vector<TypeConstraint> constraints{constraint};

  Unifier bridge(constraints);
  REQUIRE_NOTHROW(bridge.unify(tipFunctionA, tipFunctionB));
}

TEST_CASE("Unifier: Test unifying proper types with a type variable",
          "[Unifier]") {
  ASTVariableExpr variableExpr("foo");
  auto tipVar = std::make_shared<TopVar>(&variableExpr);
  auto tipInt = std::make_shared<TopInt>();

  TypeConstraint constraint(tipVar, tipInt);
  std::vector<TypeConstraint> constraints{constraint};

  Unifier bridge(constraints);
  REQUIRE_NOTHROW(bridge.unify(tipVar, tipInt));
}

TEST_CASE("Unifier: Test unifying two different type variables",
          "[Unifier]") {
  ASTVariableExpr variableExprA("foo");
  auto tipVarA = std::make_shared<TopVar>(&variableExprA);

  ASTVariableExpr variableExprB("foo");
  auto tipVarB = std::make_shared<TopVar>(&variableExprB);

  TypeConstraint constraint(tipVarA, tipVarB);
  std::vector<TypeConstraint> constraints{constraint};

  Unifier bridge(constraints);
  REQUIRE_NOTHROW(bridge.unify(tipVarA, tipVarB));
}

TEST_CASE("Unifier: Test closing mu", "[Unifier]") {
  ASTVariableExpr variableExprG("g");
  auto theAlphaG = std::make_shared<TopAlpha>(&variableExprG);

  auto theInt = std::make_shared<TopInt>();

  ASTVariableExpr variableExprFoo("foo");
  auto theVarFoo = std::make_shared<TopVar>(&variableExprFoo);
  (void)theVarFoo; // constraint2 removed: TopMu is not a valid solver input

  ASTVariableExpr variableExprF("f");
  auto theAlphaF = std::make_shared<TopAlpha>(&variableExprF);

  std::vector<std::shared_ptr<TopType>> params{theAlphaF, theAlphaG};
  auto theFunction = std::make_shared<TopFunction>(params, theAlphaG);

  auto theMu = std::make_shared<TopMu>(theAlphaF, theFunction);

  TypeConstraint constraint1(theAlphaG, theInt);
  std::vector<TypeConstraint> constraints{constraint1};
  Unifier bridge(constraints);

  auto closed = bridge.inferred(theMu);

  std::stringstream ss;
  ss << *closed;

  REQUIRE_NOTHROW(ss.str() == "\u03bc\u03B1<f>.(\u03B1<f>,int) -> int");
}

TEST_CASE("Unifier: static type predicates", "[Unifier]") {
  ASTVariableExpr varExpr("x");

  REQUIRE(Unifier::isVar(std::make_shared<TopVar>(&varExpr)));
  REQUIRE(Unifier::isCons(std::make_shared<TopInt>()));
  REQUIRE(Unifier::isMu(std::make_shared<TopMu>(
      std::make_shared<TopAlpha>(&varExpr),
      std::make_shared<TopInt>())));
  REQUIRE(Unifier::isAlpha(std::make_shared<TopAlpha>(&varExpr)));
  REQUIRE(Unifier::isProperType(std::make_shared<TopInt>()));
  REQUIRE_FALSE(Unifier::isProperType(std::make_shared<TopVar>(&varExpr)));
}

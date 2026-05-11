#include "TipTermAdapter.h"
#include "TermInterface.h"
#include "ASTVariableExpr.h"
#include "InternalError.h"
#include "TipAbsentField.h"
#include "TipAlpha.h"
#include "TipFunction.h"
#include "TipInt.h"
#include "TipMu.h"
#include "TipRecord.h"
#include "TipRef.h"
#include "TipVar.h"

#include <catch2/catch_test_macros.hpp>
#include <memory>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static ASTVariableExpr nodeA("a");
static ASTVariableExpr nodeB("b");
static ASTVariableExpr nodeC("c");

// ---------------------------------------------------------------------------
// isVariable()
// ---------------------------------------------------------------------------

TEST_CASE("TipTermAdapter: isVariable for TipVar", "[TipTermAdapter]") {
  auto var = std::make_shared<TipVar>(&nodeA);
  REQUIRE(TipTermAdapter::wrap(var)->isVariable());
}

TEST_CASE("TipTermAdapter: isVariable for TipAlpha", "[TipTermAdapter]") {
  // TipAlpha::isVariable() returns false (skolem constant in TIP type theory),
  // but the adapter returns true because the solver must bind it.
  auto alpha = std::make_shared<TipAlpha>(&nodeA);
  REQUIRE(TipTermAdapter::wrap(alpha)->isVariable());
}

TEST_CASE("TipTermAdapter: isVariable false for TipInt", "[TipTermAdapter]") {
  REQUIRE_FALSE(TipTermAdapter::wrap(std::make_shared<TipInt>())->isVariable());
}

TEST_CASE("TipTermAdapter: isVariable false for TipRef", "[TipTermAdapter]") {
  auto inner = std::make_shared<TipInt>();
  REQUIRE_FALSE(TipTermAdapter::wrap(std::make_shared<TipRef>(inner))->isVariable());
}

TEST_CASE("TipTermAdapter: isVariable false for TipFunction", "[TipTermAdapter]") {
  std::vector<std::shared_ptr<TipType>> params;
  auto fn = std::make_shared<TipFunction>(params, std::make_shared<TipInt>());
  REQUIRE_FALSE(TipTermAdapter::wrap(fn)->isVariable());
}

TEST_CASE("TipTermAdapter: isVariable false for TipAbsentField", "[TipTermAdapter]") {
  REQUIRE_FALSE(TipTermAdapter::wrap(std::make_shared<TipAbsentField>())->isVariable());
}

// ---------------------------------------------------------------------------
// wrap() guard: TipMu must be rejected
// ---------------------------------------------------------------------------

TEST_CASE("TipTermAdapter: wrap(TipMu) throws InternalError", "[TipTermAdapter]") {
  auto alpha = std::make_shared<TipAlpha>(&nodeA);
  auto inner = std::make_shared<TipInt>();
  auto mu = std::make_shared<TipMu>(alpha, inner);
  REQUIRE_THROWS_AS(TipTermAdapter::wrap(mu), InternalError);
}

// ---------------------------------------------------------------------------
// getFunctor() and arity()
// ---------------------------------------------------------------------------

TEST_CASE("TipTermAdapter: getFunctor and arity for TipVar", "[TipTermAdapter]") {
  auto var = std::make_shared<TipVar>(&nodeA);
  auto adapter = TipTermAdapter::wrap(var);
  REQUIRE(adapter->getFunctor() == var->getFunctor());
  REQUIRE(adapter->arity() == 0);
}

TEST_CASE("TipTermAdapter: getFunctor and arity for TipInt", "[TipTermAdapter]") {
  auto adapter = TipTermAdapter::wrap(std::make_shared<TipInt>());
  REQUIRE(adapter->getFunctor() == "int");
  REQUIRE(adapter->arity() == 0);
}

TEST_CASE("TipTermAdapter: getFunctor and arity for TipRef", "[TipTermAdapter]") {
  auto ref = std::make_shared<TipRef>(std::make_shared<TipInt>());
  auto adapter = TipTermAdapter::wrap(ref);
  REQUIRE(adapter->getFunctor() == "ptr");
  REQUIRE(adapter->arity() == 1);
}

TEST_CASE("TipTermAdapter: getFunctor and arity for TipFunction", "[TipTermAdapter]") {
  auto p1 = std::make_shared<TipInt>();
  auto p2 = std::make_shared<TipInt>();
  auto ret = std::make_shared<TipInt>();
  auto fn = std::make_shared<TipFunction>(
      std::vector<std::shared_ptr<TipType>>{p1, p2}, ret);
  auto adapter = TipTermAdapter::wrap(fn);
  REQUIRE(adapter->getFunctor() == "->");
  REQUIRE(adapter->arity() == 3); // 2 params + 1 return
}

// ---------------------------------------------------------------------------
// getSubterms()
// ---------------------------------------------------------------------------

TEST_CASE("TipTermAdapter: getSubterms empty for TipVar", "[TipTermAdapter]") {
  auto var = std::make_shared<TipVar>(&nodeA);
  REQUIRE(TipTermAdapter::wrap(var)->getSubterms().empty());
}

TEST_CASE("TipTermAdapter: getSubterms empty for TipAlpha", "[TipTermAdapter]") {
  auto alpha = std::make_shared<TipAlpha>(&nodeA);
  REQUIRE(TipTermAdapter::wrap(alpha)->getSubterms().empty());
}

TEST_CASE("TipTermAdapter: getSubterms for TipRef", "[TipTermAdapter]") {
  auto inner = std::make_shared<TipInt>();
  auto ref = std::make_shared<TipRef>(inner);
  auto subs = TipTermAdapter::wrap(ref)->getSubterms();
  REQUIRE(subs.size() == 1);
  auto recovered = TipTermAdapter::unwrap(subs[0]);
  REQUIRE(*recovered == *inner);
}

TEST_CASE("TipTermAdapter: getSubterms for TipFunction with two params",
          "[TipTermAdapter]") {
  auto p1 = std::make_shared<TipVar>(&nodeA);
  auto p2 = std::make_shared<TipVar>(&nodeB);
  auto ret = std::make_shared<TipInt>();
  auto fn = std::make_shared<TipFunction>(
      std::vector<std::shared_ptr<TipType>>{p1, p2}, ret);
  auto subs = TipTermAdapter::wrap(fn)->getSubterms();
  REQUIRE(subs.size() == 3);
  REQUIRE(*TipTermAdapter::unwrap(subs[0]) == *p1);
  REQUIRE(*TipTermAdapter::unwrap(subs[1]) == *p2);
  REQUIRE(*TipTermAdapter::unwrap(subs[2]) == *ret);
}

TEST_CASE("TipTermAdapter: getSubterms for TipRecord", "[TipTermAdapter]") {
  auto f1 = std::make_shared<TipInt>();
  auto f2 = std::make_shared<TipInt>();
  auto rec = std::make_shared<TipRecord>(
      std::vector<std::shared_ptr<TipType>>{f1, f2},
      std::vector<std::string>{"x", "y"});
  auto subs = TipTermAdapter::wrap(rec)->getSubterms();
  REQUIRE(subs.size() == 2);
}

// ---------------------------------------------------------------------------
// Round-trip: wrap → getSubterms → withSubterms → unwrap
// ---------------------------------------------------------------------------

TEST_CASE("TipTermAdapter: round-trip withSubterms for TipRef",
          "[TipTermAdapter]") {
  auto inner = std::make_shared<TipInt>();
  auto ref = std::make_shared<TipRef>(inner);
  auto adapter = TipTermAdapter::wrap(ref);
  auto subs = adapter->getSubterms();
  auto rebuilt = std::dynamic_pointer_cast<TipTermAdapter>(
      adapter->withSubterms(subs));
  REQUIRE(rebuilt != nullptr);
  REQUIRE(*rebuilt->getTipType() == *ref);
}

TEST_CASE("TipTermAdapter: round-trip withSubterms for TipFunction",
          "[TipTermAdapter]") {
  auto p1 = std::make_shared<TipInt>();
  auto ret = std::make_shared<TipInt>();
  auto fn = std::make_shared<TipFunction>(
      std::vector<std::shared_ptr<TipType>>{p1}, ret);
  auto adapter = TipTermAdapter::wrap(fn);
  auto subs = adapter->getSubterms();
  auto rebuilt = std::dynamic_pointer_cast<TipTermAdapter>(
      adapter->withSubterms(subs));
  REQUIRE(rebuilt != nullptr);
  REQUIRE(*rebuilt->getTipType() == *fn);
}

// ---------------------------------------------------------------------------
// equals()
// ---------------------------------------------------------------------------

TEST_CASE("TipTermAdapter: equals same type", "[TipTermAdapter]") {
  auto a = TipTermAdapter::wrap(std::make_shared<TipInt>());
  auto b = TipTermAdapter::wrap(std::make_shared<TipInt>());
  REQUIRE(a->equals(*b));
}

TEST_CASE("TipTermAdapter: equals different types", "[TipTermAdapter]") {
  auto a = TipTermAdapter::wrap(std::make_shared<TipInt>());
  auto b = TipTermAdapter::wrap(
      std::make_shared<TipRef>(std::make_shared<TipInt>()));
  REQUIRE_FALSE(a->equals(*b));
}

TEST_CASE("TipTermAdapter: equals same TipVar", "[TipTermAdapter]") {
  auto a = TipTermAdapter::wrap(std::make_shared<TipVar>(&nodeA));
  auto b = TipTermAdapter::wrap(std::make_shared<TipVar>(&nodeA));
  REQUIRE(a->equals(*b));
}

TEST_CASE("TipTermAdapter: equals different TipVar", "[TipTermAdapter]") {
  auto a = TipTermAdapter::wrap(std::make_shared<TipVar>(&nodeA));
  auto b = TipTermAdapter::wrap(std::make_shared<TipVar>(&nodeB));
  REQUIRE_FALSE(a->equals(*b));
}

// ---------------------------------------------------------------------------
// unwrap() error path
// ---------------------------------------------------------------------------

TEST_CASE("TipTermAdapter: unwrap non-adapter term throws", "[TipTermAdapter]") {
  // Build a minimal Term that is NOT a TipTermAdapter; unwrap must throw.
  struct MinimalTerm : public Term {
    bool isVariable() const override { return false; }
    std::string getFunctor() const override { return "test"; }
    std::size_t arity() const override { return 0; }
    std::vector<std::shared_ptr<Term>> getSubterms() const override { return {}; }
    std::shared_ptr<Term> withSubterms(std::vector<std::shared_ptr<Term>>) const override {
      return std::make_shared<MinimalTerm>();
    }
    std::string toString() const override { return "test"; }
    bool equals(const Term &) const override { return false; }
  };
  auto bareterm = std::make_shared<MinimalTerm>();
  REQUIRE_THROWS_AS(TipTermAdapter::unwrap(bareterm), InternalError);
}

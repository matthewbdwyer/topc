#include "TopTermAdapter.h"
#include "TermInterface.h"
#include "ASTVariableExpr.h"
#include "InternalError.h"
#include "TopAlpha.h"
#include "TopFunction.h"
#include "TopInt.h"
#include "TopMu.h"
#include "TopOwningRef.h"
#include "TopVar.h"

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

TEST_CASE("TopTermAdapter: isVariable for TopVar", "[TopTermAdapter]") {
  auto var = std::make_shared<TopVar>(&nodeA);
  REQUIRE(TopTermAdapter::wrap(var)->isVariable());
}

TEST_CASE("TopTermAdapter: isVariable for TopAlpha", "[TopTermAdapter]") {
  // TopAlpha::isVariable() returns false (skolem constant in TIP type theory),
  // but the adapter returns true because the solver must bind it.
  auto alpha = std::make_shared<TopAlpha>(&nodeA);
  REQUIRE(TopTermAdapter::wrap(alpha)->isVariable());
}

TEST_CASE("TopTermAdapter: isVariable false for TopInt", "[TopTermAdapter]") {
  REQUIRE_FALSE(TopTermAdapter::wrap(std::make_shared<TopInt>())->isVariable());
}

TEST_CASE("TopTermAdapter: isVariable false for TopOwningRef", "[TopTermAdapter]") {
  auto inner = std::make_shared<TopInt>();
  REQUIRE_FALSE(TopTermAdapter::wrap(std::make_shared<TopOwningRef>(inner))->isVariable());
}

TEST_CASE("TopTermAdapter: isVariable false for TopFunction", "[TopTermAdapter]") {
  std::vector<std::shared_ptr<TopType>> params;
  auto fn = std::make_shared<TopFunction>(params, std::make_shared<TopInt>());
  REQUIRE_FALSE(TopTermAdapter::wrap(fn)->isVariable());
}

// ---------------------------------------------------------------------------
// wrap() guard: TopMu must be rejected
// ---------------------------------------------------------------------------

TEST_CASE("TopTermAdapter: wrap(TopMu) throws InternalError", "[TopTermAdapter]") {
  auto alpha = std::make_shared<TopAlpha>(&nodeA);
  auto inner = std::make_shared<TopInt>();
  auto mu = std::make_shared<TopMu>(alpha, inner);
  REQUIRE_THROWS_AS(TopTermAdapter::wrap(mu), InternalError);
}

// ---------------------------------------------------------------------------
// getFunctor() and arity()
// ---------------------------------------------------------------------------

TEST_CASE("TopTermAdapter: getFunctor and arity for TopVar", "[TopTermAdapter]") {
  auto var = std::make_shared<TopVar>(&nodeA);
  auto adapter = TopTermAdapter::wrap(var);
  REQUIRE(adapter->getFunctor() == var->getFunctor());
  REQUIRE(adapter->arity() == 0);
}

TEST_CASE("TopTermAdapter: getFunctor and arity for TopInt", "[TopTermAdapter]") {
  auto adapter = TopTermAdapter::wrap(std::make_shared<TopInt>());
  REQUIRE(adapter->getFunctor() == "int");
  REQUIRE(adapter->arity() == 0);
}

TEST_CASE("TopTermAdapter: getFunctor and arity for TopOwningRef", "[TopTermAdapter]") {
  auto ref = std::make_shared<TopOwningRef>(std::make_shared<TopInt>());
  auto adapter = TopTermAdapter::wrap(ref);
  REQUIRE(adapter->getFunctor() == "ref");
  REQUIRE(adapter->arity() == 2);
}

TEST_CASE("TopTermAdapter: getFunctor and arity for TopFunction", "[TopTermAdapter]") {
  auto p1 = std::make_shared<TopInt>();
  auto p2 = std::make_shared<TopInt>();
  auto ret = std::make_shared<TopInt>();
  auto fn = std::make_shared<TopFunction>(
      std::vector<std::shared_ptr<TopType>>{p1, p2}, ret);
  auto adapter = TopTermAdapter::wrap(fn);
  REQUIRE(adapter->getFunctor() == "->");
  REQUIRE(adapter->arity() == 3); // 2 params + 1 return
}

// ---------------------------------------------------------------------------
// getSubterms()
// ---------------------------------------------------------------------------

TEST_CASE("TopTermAdapter: getSubterms empty for TopVar", "[TopTermAdapter]") {
  auto var = std::make_shared<TopVar>(&nodeA);
  REQUIRE(TopTermAdapter::wrap(var)->getSubterms().empty());
}

TEST_CASE("TopTermAdapter: getSubterms empty for TopAlpha", "[TopTermAdapter]") {
  auto alpha = std::make_shared<TopAlpha>(&nodeA);
  REQUIRE(TopTermAdapter::wrap(alpha)->getSubterms().empty());
}

TEST_CASE("TopTermAdapter: getSubterms for TopOwningRef", "[TopTermAdapter]") {
  auto inner = std::make_shared<TopInt>();
  auto ref = std::make_shared<TopOwningRef>(inner);
  auto subs = TopTermAdapter::wrap(ref)->getSubterms();
  REQUIRE(subs.size() == 2);
  auto mode = TopTermAdapter::unwrap(subs[0]);
  REQUIRE(mode->getFunctor() == "ownmode");
  auto recovered = TopTermAdapter::unwrap(subs[1]);
  REQUIRE(*recovered == *inner);
}

TEST_CASE("TopTermAdapter: getSubterms for TopFunction with two params",
          "[TopTermAdapter]") {
  auto p1 = std::make_shared<TopVar>(&nodeA);
  auto p2 = std::make_shared<TopVar>(&nodeB);
  auto ret = std::make_shared<TopInt>();
  auto fn = std::make_shared<TopFunction>(
      std::vector<std::shared_ptr<TopType>>{p1, p2}, ret);
  auto subs = TopTermAdapter::wrap(fn)->getSubterms();
  REQUIRE(subs.size() == 3);
  REQUIRE(*TopTermAdapter::unwrap(subs[0]) == *p1);
  REQUIRE(*TopTermAdapter::unwrap(subs[1]) == *p2);
  REQUIRE(*TopTermAdapter::unwrap(subs[2]) == *ret);
}

// ---------------------------------------------------------------------------
// Round-trip: wrap → getSubterms → withSubterms → unwrap
// ---------------------------------------------------------------------------

TEST_CASE("TopTermAdapter: round-trip withSubterms for TopOwningRef",
          "[TopTermAdapter]") {
  auto inner = std::make_shared<TopInt>();
  auto ref = std::make_shared<TopOwningRef>(inner);
  auto adapter = TopTermAdapter::wrap(ref);
  auto subs = adapter->getSubterms();
  auto rebuilt = std::dynamic_pointer_cast<TopTermAdapter>(
      adapter->withSubterms(subs));
  REQUIRE(rebuilt != nullptr);
  REQUIRE(*rebuilt->getTopType() == *ref);
}

TEST_CASE("TopTermAdapter: round-trip withSubterms for TopFunction",
          "[TopTermAdapter]") {
  auto p1 = std::make_shared<TopInt>();
  auto ret = std::make_shared<TopInt>();
  auto fn = std::make_shared<TopFunction>(
      std::vector<std::shared_ptr<TopType>>{p1}, ret);
  auto adapter = TopTermAdapter::wrap(fn);
  auto subs = adapter->getSubterms();
  auto rebuilt = std::dynamic_pointer_cast<TopTermAdapter>(
      adapter->withSubterms(subs));
  REQUIRE(rebuilt != nullptr);
  REQUIRE(*rebuilt->getTopType() == *fn);
}

// ---------------------------------------------------------------------------
// equals()
// ---------------------------------------------------------------------------

TEST_CASE("TopTermAdapter: equals same type", "[TopTermAdapter]") {
  auto a = TopTermAdapter::wrap(std::make_shared<TopInt>());
  auto b = TopTermAdapter::wrap(std::make_shared<TopInt>());
  REQUIRE(a->equals(*b));
}

TEST_CASE("TopTermAdapter: equals different types", "[TopTermAdapter]") {
  auto a = TopTermAdapter::wrap(std::make_shared<TopInt>());
  auto b = TopTermAdapter::wrap(
      std::make_shared<TopOwningRef>(std::make_shared<TopInt>()));
  REQUIRE_FALSE(a->equals(*b));
}

TEST_CASE("TopTermAdapter: equals same TopVar", "[TopTermAdapter]") {
  auto a = TopTermAdapter::wrap(std::make_shared<TopVar>(&nodeA));
  auto b = TopTermAdapter::wrap(std::make_shared<TopVar>(&nodeA));
  REQUIRE(a->equals(*b));
}

TEST_CASE("TopTermAdapter: equals different TopVar", "[TopTermAdapter]") {
  auto a = TopTermAdapter::wrap(std::make_shared<TopVar>(&nodeA));
  auto b = TopTermAdapter::wrap(std::make_shared<TopVar>(&nodeB));
  REQUIRE_FALSE(a->equals(*b));
}

// ---------------------------------------------------------------------------
// unwrap() error path
// ---------------------------------------------------------------------------

TEST_CASE("TopTermAdapter: unwrap non-adapter term throws", "[TopTermAdapter]") {
  // Build a minimal Term that is NOT a TopTermAdapter; unwrap must throw.
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
  REQUIRE_THROWS_AS(TopTermAdapter::unwrap(bareterm), InternalError);
}

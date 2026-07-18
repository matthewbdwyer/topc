#include "TopRecord.h"
#include "TopInt.h"
#include "TopRef.h"

#include <catch2/catch_test_macros.hpp>

#include <sstream>

TEST_CASE("TopRecord: Test getters", "[TopRecord]") {
  std::vector<std::shared_ptr<TopType>> inits{
      std::make_shared<TopInt>(),
      std::make_shared<TopRef>(std::make_shared<TopInt>())};
  std::vector<std::string> names{"foo", "bar"};
  TopRecord topRecord(inits, names);

  REQUIRE(names.size() == topRecord.getNames().size());
  REQUIRE(names.front() == topRecord.getNames().front());
  REQUIRE(names.back() == topRecord.getNames().back());
  REQUIRE(inits.size() == topRecord.getInits().size());
  REQUIRE(dynamic_cast<const TopInt *>(topRecord.getInits().front().get()));
  REQUIRE(dynamic_cast<const TopRef *>(topRecord.getInits().back().get()));
}

TEST_CASE("TopRecord: Test arity", "[TopRecord]") {
  std::vector<std::shared_ptr<TopType>> inits{
      std::make_shared<TopInt>(), std::make_shared<TopInt>(),
      std::make_shared<TopInt>(), std::make_shared<TopInt>(),
      std::make_shared<TopInt>(),
  };
  std::vector<std::string> names{"foo", "bar", "baz", "freddie", "fannie"};
  TopRecord topRecord(inits, names);

  REQUIRE(5 == topRecord.arity());
}

TEST_CASE("TopRecord: Test equality", "[TopRecord]") {
  std::vector<std::shared_ptr<TopType>> initsA{
      std::make_shared<TopInt>(),
      std::make_shared<TopRef>(std::make_shared<TopInt>())};
  std::vector<std::string> namesA{"foo", "bar"};
  TopRecord topRecordA(initsA, namesA);

  SECTION("Equal when fields are of same type and length") {
    std::vector<std::shared_ptr<TopType>> initsB{
        std::make_shared<TopInt>(),
        std::make_shared<TopRef>(std::make_shared<TopInt>())};
    std::vector<std::string> namesB{"foo", "bar"};
    TopRecord topRecordB(initsB, namesB);

    REQUIRE(topRecordA == topRecordB);
  }

  SECTION("Not equal when arguments differ by length") {
    std::vector<std::shared_ptr<TopType>> initsB{
        std::make_shared<TopInt>(),
        std::make_shared<TopRef>(std::make_shared<TopInt>()),
        std::make_shared<TopRef>(std::make_shared<TopInt>())};
    std::vector<std::string> namesB{"foo", "bar"};
    TopRecord topRecordB(initsB, namesB);

    REQUIRE(topRecordA != topRecordB);
  }

  SECTION("Not equal when arguments differ by type") {
    std::vector<std::shared_ptr<TopType>> initsB{
        std::make_shared<TopInt>(),
        std::make_shared<TopInt>(),
    };
    std::vector<std::string> namesB{"foo", "bar"};
    TopRecord topRecordB(initsB, namesB);

    REQUIRE(topRecordA != topRecordB);
  }
}

TEST_CASE("TopRecord: Test output stream", "[TopRecord]") {
  std::vector<std::shared_ptr<TopType>> inits{
      std::make_shared<TopInt>(),
      std::make_shared<TopRef>(std::make_shared<TopInt>())};
  std::vector<std::string> names{"foo", "bar"};
  TopRecord topRecord(inits, names);

  std::stringstream stream;
  stream << topRecord;

  REQUIRE(std::string("{foo:int,bar:⭡int}") == stream.str());
}
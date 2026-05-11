#pragma once

#include "TermInterface.h"
#include <sstream>
#include <stdexcept>
#include <utility>

class MockVar : public Term {
  std::string name;
public:
  explicit MockVar(std::string n) : name(std::move(n)) {}
  bool isVariable() const override { return true; }
  std::string getFunctor() const override { return name; }
  std::size_t arity() const override { return 0; }
  std::vector<std::shared_ptr<Term>> getSubterms() const override { return {}; }
  std::shared_ptr<Term> withSubterms(std::vector<std::shared_ptr<Term>> newSubterms) const override {
    if (!newSubterms.empty()) throw std::invalid_argument("MockVar has no subterms");
    return std::make_shared<MockVar>(name);
  }
  std::string toString() const override { return name; }
  bool equals(const Term &other) const override {
    auto *v = dynamic_cast<const MockVar *>(&other);
    return v && v->name == name;
  }
};

class MockCons : public Term {
  std::string functor;
  std::vector<std::shared_ptr<Term>> args;
public:
  MockCons(std::string f, std::vector<std::shared_ptr<Term>> a = {})
      : functor(std::move(f)), args(std::move(a)) {}
  bool isVariable() const override { return false; }
  std::string getFunctor() const override { return functor; }
  std::size_t arity() const override { return args.size(); }
  std::vector<std::shared_ptr<Term>> getSubterms() const override { return args; }
  std::shared_ptr<Term> withSubterms(std::vector<std::shared_ptr<Term>> newSubterms) const override {
    if (newSubterms.size() != args.size()) throw std::invalid_argument("Wrong number of subterms");
    return std::make_shared<MockCons>(functor, std::move(newSubterms));
  }
  std::string toString() const override {
    if (args.empty()) return functor;
    std::ostringstream os;
    os << functor << "(";
    for (std::size_t i = 0; i < args.size(); ++i) {
      if (i > 0) os << ", ";
      os << args[i]->toString();
    }
    os << ")";
    return os.str();
  }
  bool equals(const Term &other) const override {
    auto *c = dynamic_cast<const MockCons *>(&other);
    if (!c || c->functor != functor || c->args.size() != args.size()) return false;
    for (std::size_t i = 0; i < args.size(); ++i) {
      if (!args[i]->equals(*c->args[i])) return false;
    }
    return true;
  }
};

// SimpleTermImpl.h — no TIP includes, used only by standalone solver tests
#pragma once
#include "TermInterface.h"
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

/** A variable term: isVariable()=true, arity=0. */
class SimpleVar : public Term {
  std::string name;
public:
  explicit SimpleVar(std::string n) : name(std::move(n)) {}
  bool isVariable() const override { return true; }
  std::string getFunctor() const override { return name; }
  std::size_t arity() const override { return 0; }
  std::vector<std::shared_ptr<Term>> getSubterms() const override { return {}; }
  std::shared_ptr<Term> withSubterms(std::vector<std::shared_ptr<Term>>) const override {
    return std::make_shared<SimpleVar>(name);
  }
  std::string toString() const override { return "?" + name; }
  bool equals(const Term &other) const override {
    auto *o = dynamic_cast<const SimpleVar *>(&other);
    return o && o->name == name;
  }
};

/** A proper term (constructor): isVariable()=false, arity=subterms.size(). */
class SimpleCons : public Term {
  std::string functor;
  std::vector<std::shared_ptr<Term>> subs;
public:
  SimpleCons(std::string f, std::vector<std::shared_ptr<Term>> s)
      : functor(std::move(f)), subs(std::move(s)) {}
  bool isVariable() const override { return false; }
  std::string getFunctor() const override { return functor; }
  std::size_t arity() const override { return subs.size(); }
  std::vector<std::shared_ptr<Term>> getSubterms() const override { return subs; }
  std::shared_ptr<Term> withSubterms(std::vector<std::shared_ptr<Term>> ns) const override {
    return std::make_shared<SimpleCons>(functor, std::move(ns));
  }
  std::string toString() const override {
    std::string r = functor + "(";
    for (size_t i = 0; i < subs.size(); ++i) r += (i ? "," : "") + subs[i]->toString();
    return r + ")";
  }
  bool equals(const Term &other) const override {
    auto *o = dynamic_cast<const SimpleCons *>(&other);
    if (!o || o->functor != functor || o->subs.size() != subs.size()) return false;
    for (size_t i = 0; i < subs.size(); ++i)
      if (!subs[i]->equals(*o->subs[i])) return false;
    return true;
  }
};

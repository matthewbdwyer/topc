#pragma once

#include "TermInterface.h"
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

/**
 * @class TermUnificationError
 * @brief Exception thrown when term unification fails.
 */
class TermUnificationError : public std::runtime_error {
  std::shared_ptr<Term> lhs;
  std::shared_ptr<Term> rhs;

public:
  TermUnificationError(const std::string &msg, std::shared_ptr<Term> l,
                       std::shared_ptr<Term> r)
      : std::runtime_error(msg), lhs(std::move(l)), rhs(std::move(r)) {}

  const std::shared_ptr<Term> &getLhs() const { return lhs; }
  const std::shared_ptr<Term> &getRhs() const { return rhs; }
};

/**
 * @class TermUnifier
 * @brief Generic first-order term unification solver.
 */
class TermUnifier {
public:
  using Constraint = std::pair<std::shared_ptr<Term>, std::shared_ptr<Term>>;
  using Substitution = std::map<std::string, std::shared_ptr<Term>>;
  using CycleHandler = std::function<std::shared_ptr<Term>(
      const std::string &varName, std::shared_ptr<Term> cyclicTerm)>;

  TermUnifier() = default;

  void addConstraint(std::shared_ptr<Term> t1, std::shared_ptr<Term> t2);
  void solve();
  std::shared_ptr<Term> apply(std::shared_ptr<Term> term) const;
  const Substitution &getSubstitution() const { return substitution; }
  bool isBound(const std::string &varName) const;
  std::shared_ptr<Term> lookup(const std::string &varName) const;
  void close(CycleHandler cycleHandler = nullptr);

private:
  std::vector<Constraint> constraints;
  Substitution substitution;

  void unify(std::shared_ptr<Term> t1, std::shared_ptr<Term> t2);
  bool occursIn(const std::string &varName, const Term *term) const;
  std::shared_ptr<Term> find(std::shared_ptr<Term> term) const;
};

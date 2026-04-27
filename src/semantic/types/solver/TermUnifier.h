#pragma once

#include "TermInterface.h"
#include "TermUnionFind.h"
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
 * @brief Generic first-order term unification solver backed by union-find.
 *
 * Uses a TermUnionFind structure for symmetric equivalence-class merging,
 * mirroring the Unifier class's use of UnionFind at the TipType level.
 *
 * No occurs check is performed: cyclic constraints are allowed and produce
 * recursive types during the closure phase (TipTermClosure).
 */
class TermUnifier {
public:
  using Constraint = std::pair<std::shared_ptr<Term>, std::shared_ptr<Term>>;

  TermUnifier() = default;

  void addConstraint(std::shared_ptr<Term> t1, std::shared_ptr<Term> t2);
  void solve();

  /**
   * @brief Return the canonical representative of term's equivalence class.
   *
   * After solve(), find(v) returns the proper type that v was unified with,
   * or a term value-equal to v if v is unconstrained.
   */
  std::shared_ptr<Term> find(std::shared_ptr<Term> term);

  /**
   * @brief Fully resolve a term by substituting all known variable bindings
   *        into its subterms (acyclic bindings only).
   */
  std::shared_ptr<Term> apply(std::shared_ptr<Term> term);

private:
  std::vector<Constraint> constraints;
  TermUnionFind unionFind;

  void unify(std::shared_ptr<Term> t1, std::shared_ptr<Term> t2);
};


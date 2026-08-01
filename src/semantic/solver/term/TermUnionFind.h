#pragma once

#include "TermInterface.h"
#include <iostream>
#include <map>
#include <memory>

/**
 * @class TermUnionFind
 * @brief Generic union-find data structure over Term objects.
 *
 * Mirrors the structure of UnionFind but operates on shared_ptr<Term> using
 * Term::equals() for value-based comparison.  This allows TermUnifier to use
 * the same union-find algorithm for constraint solving that Unifier uses at
 * the TopType level, giving students a consistent pedagogical experience.
 *
 * Key invariant (same as UnionFind): when a variable is merged with a proper
 * type, the proper type becomes the canonical representative.
 */
class TermUnionFind {
public:
  TermUnionFind() = default;

  /** Add a term (idempotent by value equality via Term::equals()). */
  void add(std::shared_ptr<Term> term);

  /** Return the canonical representative of term's equivalence class. */
  std::shared_ptr<Term> find(std::shared_ptr<Term> term);

  /**
   * Merge the equivalence classes of t1 and t2.
   * t2's root becomes the representative of the merged class.
   */
  void quick_union(std::shared_ptr<Term> t1, std::shared_ptr<Term> t2);

  /** True iff t1 and t2 are in the same equivalence class. */
  bool connected(std::shared_ptr<Term> t1, std::shared_ptr<Term> t2);

  friend std::ostream &operator<<(std::ostream &os, const TermUnionFind &uf);

private:
  // Parent edges. std::map uses pointer-value ordering for storage; all
  // semantic lookups use value equality via linear scan (same as UnionFind).
  std::map<std::shared_ptr<Term>, std::shared_ptr<Term>> edges;

  /** Linear scan: find an existing edge key that is value-equal to t. */
  std::shared_ptr<Term> lookup_edge(const std::shared_ptr<Term> &t) const;

  /** Return the parent of t (t must already be in edges). */
  std::shared_ptr<Term> get_parent(const std::shared_ptr<Term> &t);

  /**
   * Insert t if no value-equal entry exists; return the canonical stored
   * pointer for this term value.  Inserts a self-loop (t is its own root)
   * for new entries.
   */
  std::shared_ptr<Term> smart_insert(std::shared_ptr<Term> t);

  std::ostream &print(std::ostream &out) const;
};

#pragma once

#include "TopType.h"
#include <iostream>
#include <memory>
#include <set>
#include <string>

class ASTNode;
class TopTypeVisitor;

/*!
 * \class TopVar
 *
 * \brief Class representing a type variable (unification variable).
 */
class TopVar : public TopType {
public:
  TopVar() = delete;
  TopVar(ASTNode *node);

  ASTNode *getNode() const;
  std::ostream &print(std::ostream &out) const override;
  bool operator==(const TopType &other) const override;
  bool operator!=(const TopType &other) const;
  bool operator<(const TopVar &other) const;

  void accept(TopTypeVisitor *visitor) override;

  // ========== Solver-facing interface ==========
  bool isVariable() const override { return true; }
  std::string getFunctor() const override;
  std::size_t arity() const override { return 0; }

  // ========== TopType structural interface ==========
  std::shared_ptr<TopType> withChildTypes(
      std::vector<std::shared_ptr<TopType>> children) const override;

protected:
  ASTNode *node;
};

/*! \brief Value comparator for sets of shared_ptr<TopVar>.
 *
 * Compares by dereferenced node pointer rather than the TopVar wrapper address,
 * giving value (not pointer-identity) set semantics.
 */
struct TopVarValueCmp {
  bool operator()(const std::shared_ptr<TopVar> &a,
                  const std::shared_ptr<TopVar> &b) const {
    return *a < *b;
  }
};

using TopVarSet = std::set<std::shared_ptr<TopVar>, TopVarValueCmp>;

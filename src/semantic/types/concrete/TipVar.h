#pragma once

#include "TipType.h"
#include <iostream>
#include <memory>
#include <set>
#include <string>

class ASTNode;
class TipTypeVisitor;

/*!
 * \class TipVar
 *
 * \brief Class representing a type variable (unification variable).
 */
class TipVar : public TipType {
public:
  TipVar() = delete;
  TipVar(ASTNode *node);

  ASTNode *getNode() const;
  std::ostream &print(std::ostream &out) const override;
  bool operator==(const TipType &other) const override;
  bool operator!=(const TipType &other) const;
  bool operator<(const TipVar &other) const;

  void accept(TipTypeVisitor *visitor) override;

  // ========== Term interface ==========
  bool isVariable() const override { return true; }
  std::string getFunctor() const override;
  std::size_t arity() const override { return 0; }
  std::vector<std::shared_ptr<Term>> getSubterms() const override { return {}; }
  std::shared_ptr<Term> withSubterms(std::vector<std::shared_ptr<Term>> newSubterms) const override;

protected:
  ASTNode *node;
};

/*! \brief Value comparator for sets of shared_ptr<TipVar>.
 *
 * Compares by dereferenced node pointer rather than the TipVar wrapper address,
 * giving value (not pointer-identity) set semantics.
 */
struct TipVarValueCmp {
  bool operator()(const std::shared_ptr<TipVar> &a,
                  const std::shared_ptr<TipVar> &b) const {
    return *a < *b;
  }
};

using TipVarSet = std::set<std::shared_ptr<TipVar>, TipVarValueCmp>;

#pragma once

#include "TipVar.h"
#include <iostream>
#include <string>

class TipTypeVisitor;

/*!
 * \class TipAlpha
 *
 * \brief A type variable used in type schemes (not a unification variable).
 */
class TipAlpha : public TipVar {
public:
  TipAlpha() = delete;
  TipAlpha(ASTNode *node);
  TipAlpha(ASTNode *node, std::string const name);
  TipAlpha(ASTNode *node, ASTNode *context, std::string const name);

  ASTNode *getContext() const;
  std::string const &getName() const;

  bool operator==(const TipType &other) const override;
  bool operator!=(const TipType &other) const;

  void accept(TipTypeVisitor *visitor) override;

  // ========== Solver-facing interface ==========
  bool isVariable() const override { return false; }
  std::string getFunctor() const override;
  std::size_t arity() const override { return 0; }

  // ========== TipType structural interface ==========
  std::shared_ptr<TipType> withChildTypes(
      std::vector<std::shared_ptr<TipType>> children) const override;

protected:
  std::ostream &print(std::ostream &out) const override;

private:
  ASTNode *context;
  std::string name;
};

#pragma once

#include "TopVar.h"
#include <iostream>
#include <string>

class TopTypeVisitor;

/*!
 * \class TopAlpha
 *
 * \brief A type variable used in type schemes (not a unification variable).
 */
class TopAlpha : public TopVar {
public:
  TopAlpha() = delete;
  TopAlpha(ASTNode *node);
  TopAlpha(ASTNode *node, std::string const name);
  TopAlpha(ASTNode *node, ASTNode *context, std::string const name);

  ASTNode *getContext() const;
  std::string const &getName() const;

  bool operator==(const TopType &other) const override;
  bool operator!=(const TopType &other) const;

  void accept(TopTypeVisitor *visitor) override;

  // ========== Solver-facing interface ==========
  bool isVariable() const override { return false; }
  std::string getFunctor() const override;
  std::size_t arity() const override { return 0; }

  // ========== TopType structural interface ==========
  std::shared_ptr<TopType> withChildTypes(
      std::vector<std::shared_ptr<TopType>> children) const override;

protected:
  std::ostream &print(std::ostream &out) const override;

private:
  ASTNode *context;
  std::string name;
};

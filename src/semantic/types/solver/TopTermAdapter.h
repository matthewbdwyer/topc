#pragma once

#include "TermInterface.h"
#include "TopType.h"
#include <memory>
#include <vector>

/*!
 * \class TopTermAdapter
 *
 * \brief Adapts a shared_ptr<TopType> to the Term interface.
 *
 * This is the ONLY class in the codebase that includes both a TOP type header
 * and a solver header.  It is the exclusive bridge between the two layers.
 *
 * isVariable() returns true for both TopVar and TopAlpha:
 *   - TopVar  — ordinary unification variable bound to an AST node
 *   - TopAlpha — skolem introduced during polymorphic instantiation;
 *                the solver must bind it to a concrete type, so from the
 *                solver's perspective it is a variable even though
 *                TopAlpha::isVariable() returns false (correct for TOP
 *                type-theory semantics).
 *
 * wrap() rejects TopMu: TopMu is a closure-phase output, not a valid
 * solver input.  Passing one throws InternalError immediately so the
 * invariant is enforced at the boundary.
 */
class TopTermAdapter : public Term {
public:
  /*!
   * \brief Wrap a TopType as a Term.
   * \throws InternalError if t is a TopMu.
   */
  static std::shared_ptr<TopTermAdapter> wrap(std::shared_ptr<TopType> t);

  /*!
   * \brief Recover the TopType from a Term returned by TermUnifier.
   * \throws InternalError if t is not a TopTermAdapter.
   */
  static std::shared_ptr<TopType> unwrap(std::shared_ptr<Term> t);

  std::shared_ptr<TopType> getTopType() const { return topType; }

  // ── Term interface ──────────────────────────────────────────────────────

  /*!
   * Returns true for TopVar and TopAlpha (both are bindable in the solver).
   * dynamic_pointer_cast<TopVar> catches TopAlpha too because TopAlpha IS-A TopVar.
   */
  bool isVariable() const override;
  std::string getFunctor() const override;
  std::size_t arity() const override;
  std::vector<std::shared_ptr<Term>> getSubterms() const override;
  std::shared_ptr<Term> withSubterms(
      std::vector<std::shared_ptr<Term>> newSubterms) const override;
  std::string toString() const override;
  bool equals(const Term &other) const override;

private:
  explicit TopTermAdapter(std::shared_ptr<TopType> t);
  std::shared_ptr<TopType> topType;
};

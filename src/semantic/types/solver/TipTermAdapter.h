#pragma once

#include "TermInterface.h"
#include "TipType.h"
#include <memory>
#include <vector>

/*!
 * \class TipTermAdapter
 *
 * \brief Adapts a shared_ptr<TipType> to the Term interface.
 *
 * This is the ONLY class in the codebase that includes both a TIP type header
 * and a solver header.  It is the exclusive bridge between the two layers.
 *
 * isVariable() returns true for both TipVar and TipAlpha:
 *   - TipVar  — ordinary unification variable bound to an AST node
 *   - TipAlpha — skolem introduced during polymorphic instantiation;
 *                the solver must bind it to a concrete type, so from the
 *                solver's perspective it is a variable even though
 *                TipAlpha::isVariable() returns false (correct for TIP
 *                type-theory semantics).
 *
 * wrap() rejects TipMu: TipMu is a closure-phase output, not a valid
 * solver input.  Passing one throws InternalError immediately so the
 * invariant is enforced at the boundary.
 */
class TipTermAdapter : public Term {
public:
  /*!
   * \brief Wrap a TipType as a Term.
   * \throws InternalError if t is a TipMu.
   */
  static std::shared_ptr<TipTermAdapter> wrap(std::shared_ptr<TipType> t);

  /*!
   * \brief Recover the TipType from a Term returned by TermUnifier.
   * \throws InternalError if t is not a TipTermAdapter.
   */
  static std::shared_ptr<TipType> unwrap(std::shared_ptr<Term> t);

  std::shared_ptr<TipType> getTipType() const { return tipType; }

  // ── Term interface ──────────────────────────────────────────────────────

  /*!
   * Returns true for TipVar and TipAlpha (both are bindable in the solver).
   * dynamic_pointer_cast<TipVar> catches TipAlpha too because TipAlpha IS-A TipVar.
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
  explicit TipTermAdapter(std::shared_ptr<TipType> t);
  std::shared_ptr<TipType> tipType;
};

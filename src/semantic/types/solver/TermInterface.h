#pragma once

#include <memory>
#include <string>
#include <vector>

/**
 * @class Term
 * @brief Abstract interface for terms in a first-order term algebra.
 *
 * This interface defines the minimal operations required for unification.
 * Any type system that wants to use the Unifier must implement this interface.
 *
 * A term is either:
 * - A variable (isVariable() returns true)
 * - A compound term with a functor and zero or more subterms
 *
 * The unification algorithm only depends on this interface, not on any
 * concrete type representation.
 */
class Term : public std::enable_shared_from_this<Term> {
public:
  virtual ~Term() = default;

  /**
   * @brief Check if this term is a unification variable.
   * @return true if this is a variable that can be bound during unification.
   */
  virtual bool isVariable() const = 0;

  /**
   * @brief Check if this term is a proper type (not a variable).
   * @return true if this is a concrete type constructor.
   */
  virtual bool isProper() const { return !isVariable(); }

  /**
   * @brief Get the functor (constructor name) of this term.
   * @return A string identifying the term constructor (e.g., "int", "->", "ptr").
   *
   * For variables, this may return a unique identifier or empty string.
   */
  virtual std::string getFunctor() const = 0;

  /**
   * @brief Get the arity (number of subterms) of this term.
   * @return The number of immediate subterms.
   *
   * Variables have arity 0. Nullary constructors (like "int") also have arity 0.
   */
  virtual std::size_t arity() const = 0;

  /**
   * @brief Get the subterms of this term.
   * @return A vector of shared pointers to the immediate subterms.
   *
   * For a function type A -> B, this returns [A, B].
   * For variables and nullary constructors, this returns an empty vector.
   */
  virtual std::vector<std::shared_ptr<Term>> getSubterms() const = 0;

  /**
   * @brief Create a new term with the same functor but different subterms.
   * @param newSubterms The new subterms to use.
   * @return A new term with the same structure but substituted subterms.
   * @throws std::invalid_argument if newSubterms.size() != arity()
   *
   * This is used by the unifier to apply substitutions.
   */
  virtual std::shared_ptr<Term>
  withSubterms(std::vector<std::shared_ptr<Term>> newSubterms) const = 0;

  /**
   * @brief Get a string representation for error messages and debugging.
   * @return A human-readable string representation of this term.
   */
  virtual std::string toString() const = 0;

  /**
   * @brief Check if two terms have the same functor and arity.
   * @param other The term to compare with.
   * @return true if both terms have matching structure (ignoring subterms).
   *
   * Default implementation compares functor and arity.
   */
  virtual bool matchesFunctor(const Term &other) const {
    return getFunctor() == other.getFunctor() && arity() == other.arity();
  }

  /**
   * @brief Check structural equality of two terms.
   * @param other The term to compare with.
   * @return true if the terms are structurally identical.
   */
  virtual bool equals(const Term &other) const = 0;
};

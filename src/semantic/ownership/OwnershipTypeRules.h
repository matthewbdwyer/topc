#pragma once

#include <string>

class ASTProgram;
class SymbolTable;
class TopType;
class TypeInference;

/*! \class OwnershipTypeRules
 *  \brief Ownership rules that are read off solved types (kind: read-off).
 *
 * Every rule here looks at a type and nothing else, so it needs no traversal
 * context and no program-point state:
 *
 *  - an owning pointer's payload is Copy: `alloc E` may not own an owned value
 *    (`own&own` does not exist), which is what makes `*p` on an owning pointer
 *    a Copy read;
 *  - a borrow is not a component: no borrow mode inside a sum-type payload, an
 *    `alloc` payload, or a function's result, because any of those outlives
 *    the call that received the borrow;
 *  - recursive function types (a function type under a `mu`) are not
 *    supported by the ownership analyses.
 *
 * The checks run at different points of the pipeline so that, where several
 * rules would reject a program, the most specific diagnostic wins: alloc
 * payloads right after type inference; borrow components after the
 * interprocedural borrow-flow check; recursive types while summaries are
 * built. The type predicates are shared with the other ownership passes.
 */
class OwnershipTypeRules {
public:
  /*! \brief Reject an `alloc` whose payload is an owned value. */
  static void checkAllocPayloads(ASTProgram *p, TypeInference *types);

  /*! \brief Reject a borrow inside a sum payload, an `alloc` payload, or a
   *  function result. */
  static void checkBorrowComponents(ASTProgram *p, SymbolTable *sym,
                                    TypeInference *types);

  /*! \brief Reject \p type if it contains a recursive function type;
   *  \p context names where it occurs in the diagnostic. */
  static void rejectUnsupportedRecursiveType(TopType *type,
                                             const std::string &context);

  /*! \brief True if a borrow mode occurs anywhere in \p type, not looking
   *  inside function types (a stored function value owns nothing). */
  static bool containsBorrow(const TopType *type);

  /*! \brief True if \p type still depends on instantiation: a free type
   *  variable or an unresolved reference mode (the bound variable of a
   *  recursive type does not count). */
  static bool containsTypeVariable(TopType *type);
};

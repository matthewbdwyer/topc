#pragma once

#include "TopCons.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

class TopTypeVisitor;

/*!
 * \class TopSumType
 *
 * \brief A proper type representing a sum (variant/tagged-union) type.
 *
 * Stores the constructor names in declaration order plus the payload type
 * variables for each constructor.  Constructors with no parameters
 * contribute zero arguments; constructors with n parameters contribute
 * n arguments to the flat `arguments` vector inherited from TopCons.
 *
 * Two TopSumType terms unify when they have the same type name (enforced
 * by getFunctor) and their flat argument vectors unify element-wise.
 */
class TopSumType : public TopCons {
  std::string typeName;
  std::vector<std::string> ctorOrder;     ///< constructor names in declaration order
  std::map<std::string, int> ctorArities; ///< ctor tag -> payload count

public:
  TopSumType() = delete;

  /*!
   * \param name     The declared type name (e.g. "Option").
   * \param ctors    Constructor names in declaration order.
   * \param payloads All payload type arguments, concatenated in ctor order.
   * \param arities  Map from each ctor name to its arity (number of payloads).
   */
  TopSumType(std::string name,
             std::vector<std::string> ctors,
             std::vector<std::shared_ptr<TopType>> payloads,
             std::map<std::string, int> arities);

  const std::string &getTypeName() const { return typeName; }
  const std::vector<std::string> &getCtorOrder() const { return ctorOrder; }
  const std::map<std::string, int> &getCtorArities() const { return ctorArities; }

  /*! Return the payload types for a specific constructor. */
  std::vector<std::shared_ptr<TopType>>
  getCtorPayloads(const std::string &tag) const;

  bool operator==(const TopType &other) const override;
  bool operator!=(const TopType &other) const;

  void accept(TopTypeVisitor *visitor) override;

  // ========== Solver-facing interface ==========
  std::string getFunctor() const override { return "sumtype_" + typeName; }
  std::size_t arity() const override { return arguments.size(); }

  // ========== TopType structural interface ==========
  std::shared_ptr<TopType> withChildTypes(
      std::vector<std::shared_ptr<TopType>> children) const override;

protected:
  std::ostream &print(std::ostream &out) const override;
};

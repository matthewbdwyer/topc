#pragma once

#include "ReferenceType.h"
#include <memory>
#include <string>
#include <vector>

class TopTypeVisitor;

/*!
 * \class TopOwningRef
 *
 * \brief A proper type representing an owning (heap-allocated) pointer type.
 *
 * In TOP programs, `alloc E` produces an owning heap reference.
 */
class TopOwningRef : public ReferenceType {
public:
  TopOwningRef() = delete;
  TopOwningRef(std::shared_ptr<TopType> of);

  std::shared_ptr<TopType> getReferencedType() const;

  bool operator==(const TopType &other) const override;
  bool operator!=(const TopType &other) const;

  void accept(TopTypeVisitor *visitor) override;

  // ========== Solver-facing interface ==========
  std::string getFunctor() const override { return "ref"; }
  std::size_t arity() const override { return 2; }

  // ========== TopType structural interface ==========
  std::shared_ptr<TopType> withChildTypes(
      std::vector<std::shared_ptr<TopType>> children) const override;

protected:
  std::ostream &print(std::ostream &out) const override;
};

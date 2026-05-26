#pragma once

#include "TopCons.h"
#include <memory>
#include <string>
#include <vector>

class TopTypeVisitor;

/*!
 * \class TopOwningRef
 *
 * \brief A proper type representing an owning (heap-allocated) pointer type.
 *
 * In TOP programs, `alloc E` produces a `TopOwningRef` rather than a `TopRef`.
 */
class TopOwningRef : public TopCons {
public:
  TopOwningRef() = delete;
  TopOwningRef(std::shared_ptr<TopType> of);

  std::shared_ptr<TopType> getReferencedType() const;

  bool operator==(const TopType &other) const override;
  bool operator!=(const TopType &other) const;

  void accept(TopTypeVisitor *visitor) override;

  // ========== Solver-facing interface ==========
  std::string getFunctor() const override { return "ownref"; }
  std::size_t arity() const override { return 1; }

  // ========== TopType structural interface ==========
  std::shared_ptr<TopType> withChildTypes(
      std::vector<std::shared_ptr<TopType>> children) const override;

protected:
  std::ostream &print(std::ostream &out) const override;
};

#include "TopTypeClosure.h"

#include "InternalError.h"
#include "Substituter.h"
#include "TopAlpha.h"
#include "TopModeVar.h"
#include "TopMu.h"
#include "TypeVars.h"
#include "../SemanticLogging.h"
#include <memory>

// ---------------------------------------------------------------------------
// Local helpers (mirror Unifier's anonymous-namespace helpers)
// ---------------------------------------------------------------------------

namespace {

bool isVar(const std::shared_ptr<TopType> &type) {
  return std::dynamic_pointer_cast<TopVar>(type) != nullptr;
}

bool isAlpha(const std::shared_ptr<TopType> &type) {
  return std::dynamic_pointer_cast<TopAlpha>(type) != nullptr;
}

bool isMu(const std::shared_ptr<TopType> &type) {
  return std::dynamic_pointer_cast<TopMu>(type) != nullptr;
}

} // namespace

// ---------------------------------------------------------------------------
// TopTypeClosure
// ---------------------------------------------------------------------------

TopTypeClosure::TopTypeClosure(const TermUnifier &u) : unifier(u) {}

/**
 * Close a type expression by replacing all bound variables with their
 * inferred types, wrapping cyclic bindings in TopMu.
 *
 * Algorithm mirrors Unifier::close():
 *   unionFind->find(v) != v  →  !rep->equals(*v)  (value-based bound check)
 *   close(unionFind->find(v),…)  →  close(rep, …)  where rep = find(v)
 *   unionFind->add(newTypes)  →  (omitted; TermUnifier's union-find persists)
 */
std::shared_ptr<TopType> TopTypeClosure::close(std::shared_ptr<TopType> type,
                                               TopVarSet visited) {
  if (auto modeVar = std::dynamic_pointer_cast<TopModeVar>(type)) {
    auto modeAdapter = TopTermAdapter::wrap(modeVar);
    auto repTerm = const_cast<TermUnifier &>(unifier).find(modeAdapter);
    auto rep = TopTermAdapter::unwrap(repTerm);

    if (*rep == *modeVar) {
      SEMANTIC_LOG(2, "type-closure")
          << "mode=m" << modeVar->getId() << " representative=unbound"
          << " result=polymorphic";
      return std::make_shared<TopModeVar>(modeVar->getId());
    }
    if (std::dynamic_pointer_cast<TopModeVar>(rep) != nullptr) {
      throw InternalError("cyclic reference mode binding");
    }
    auto closedMode = close(rep, visited);
    SEMANTIC_LOG(2, "type-closure")
      << "mode=m" << modeVar->getId()
      << " representative=" << rep->toString()
      << " result=" << closedMode->toString();
    return closedMode;
  }

  if (isVar(type)) {
    auto v = std::dynamic_pointer_cast<TopVar>(type);

    // Ask the union-find for the canonical representative of v.
    // const_cast is needed because find() is non-const (smart_insert may add).
    auto vAdapter = TopTermAdapter::wrap(v);
    auto repTerm =
        const_cast<TermUnifier &>(unifier).find(vAdapter);
    auto rep = TopTermAdapter::unwrap(repTerm);

    // v is "bound" iff its representative is value-different from v.
    bool bound = (*rep != *v);

    if (!visited.count(v) && bound) {
      visited.insert(v);

      auto closedV = close(rep, visited);

      // Reuse existing alpha (preserving context and name); otherwise create a fresh one for this node
      auto newV = v;
      if (!isAlpha(v)) {
        newV = std::make_shared<TopAlpha>(v->getNode());
      }

      auto freeV = TypeVars::collect(closedV.get());

      if ((*closedV != *newV) && freeV.count(newV)) {
        // Cyclic reference — wrap in mu
        auto substClosedV =
            Substituter::substitute(closedV.get(), v.get(), newV);
        return std::make_shared<TopMu>(newV, substClosedV);
      } else {
        return closedV;
      }
    } else {
      // Unbound or already-visited (cycle guard) — return the original variable (preserving TopAlpha identity if present)
      if (isAlpha(v)) {
        return v;
      }
      return std::make_shared<TopAlpha>(v->getNode());
    }

  } else if (isMu(type)) {
    auto m = std::dynamic_pointer_cast<TopMu>(type);
    return std::make_shared<TopMu>(m->getV(), close(m->getT(), visited));
  }

  std::vector<std::shared_ptr<TopType>> closedChildren;
  for (const auto &child : type->getChildTypes()) {
    closedChildren.push_back(close(child, visited));
  }
  return type->withChildTypes(std::move(closedChildren));
}

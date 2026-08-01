#pragma once

#include "ASTPattern.h"

#include <memory>
#include <string>
#include <vector>

/*! \brief Pattern that matches a sum-type constructor and destructures its
 * payload.
 *
 * Examples:
 *   `Inner(x)`    — tag="Inner", one sub-pattern ASTVarPattern(x)
 *   `Outer(Inner(_))` — tag="Outer", one sub-pattern ASTCtorPattern
 *   `None`        — tag="None",  zero sub-patterns (no-arg constructor)
 */
class ASTCtorPattern : public ASTPattern {
  std::string TAG;
  std::vector<std::shared_ptr<ASTPattern>> SUB_PATTERNS;

public:
  ASTCtorPattern(std::string tag,
                 std::vector<std::shared_ptr<ASTPattern>> subPatterns)
      : TAG(std::move(tag)), SUB_PATTERNS(std::move(subPatterns)) {}

  const std::string &getTag() const { return TAG; }

  std::vector<ASTPattern *> getSubPatterns() const {
    std::vector<ASTPattern *> r;
    for (auto &s : SUB_PATTERNS)
      r.push_back(s.get());
    return r;
  }

  const std::vector<std::shared_ptr<ASTPattern>> &getSubPatternsShared() const {
    return SUB_PATTERNS;
  }

  std::ostream &print(std::ostream &out) const override;
};

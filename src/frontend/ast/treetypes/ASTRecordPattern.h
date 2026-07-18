#pragma once

#include "ASTPattern.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

/*! \brief Pattern that matches a record value and destructures its fields.
 *
 * Example: `{r: x, g: y}` in `Green({r: x, g: y})`.
 * Each field entry maps a field name to a sub-pattern.
 */
class ASTRecordPattern : public ASTPattern {
  std::vector<std::pair<std::string, std::shared_ptr<ASTPattern>>> FIELDS;

public:
  explicit ASTRecordPattern(
      std::vector<std::pair<std::string, std::shared_ptr<ASTPattern>>> fields)
      : FIELDS(std::move(fields)) {}

  const std::vector<std::pair<std::string, std::shared_ptr<ASTPattern>>> &
  getFields() const {
    return FIELDS;
  }

  std::ostream &print(std::ostream &out) const override;
};

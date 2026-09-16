#pragma once

#include <string>
#include <vector>

/*! \brief Developer switches that turn off one ownership check or safety
 *  mechanism at a time.
 *
 * Used only by the test-adequacy harness (test/system/soundness/adequacy.py):
 * with a rule disabled, the compiler is deliberately unsound, and at least one
 * test must notice. A rule no test notices is untested. Set with the hidden
 * option --unsafe-disable=<id>[,<id>...] or the TOPC_UNSAFE_DISABLE
 * environment variable. Never used in normal compilation.
 */
namespace RuleToggles {

/*! \brief Every rule identifier, in a stable order. */
const std::vector<std::string> &known();

/*! \brief Disable \p id. Returns false if the identifier is unknown. */
bool disable(const std::string &id);

/*! \brief Disable each comma-separated identifier. Returns the first unknown
 *  one, or an empty string. */
std::string disableList(const std::string &ids);

/*! \brief True unless \p id was disabled. */
bool enabled(const char *id);

/*! \brief Report a violation of rule \p id: throws SemanticError(message)
 *  unless the rule was disabled for adequacy testing, in which case it
 *  returns and the caller continues as if the program were valid. Every
 *  ownership and borrow check reports through here. */
void reject(const char *id, const std::string &message);

} // namespace RuleToggles

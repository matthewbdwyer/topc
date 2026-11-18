#pragma once

#include <stdexcept>
#include <string>
#include <vector>

namespace verutop {

// What the prototype does not handle: the function is skipped, with this reason.
struct Unsupported : std::runtime_error {
  using std::runtime_error::runtime_error;
};

inline std::string pad(int n) { return std::string(n > 0 ? n : 0, ' '); }
inline std::string join(const std::vector<std::string> &xs, const std::string &sep = ", ") {
  std::string s;
  for (size_t i = 0; i < xs.size(); ++i) s += (i ? sep : "") + xs[i];
  return s;
}

} // namespace verutop

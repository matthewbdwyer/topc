#pragma once

#include "Gcl.h"

#include <map>

namespace verutop {

// ===========================================================================
// Counterexample traces.  Z3's model gives a value to every version; the
// trace follows the passive program along the path the model takes.
// ===========================================================================

std::string baseName(const std::string &version);
// The branch conditions of a passive program.
void guards(const G &c, std::vector<T> &out);

class Trace {
public:
  Trace(const std::map<std::string, std::string> &val, int target);
  std::vector<std::string> lines;
  bool walk(const G &c);

private:
  std::map<std::string, std::string> val;
  Sub vals;
  int target;

  static const Assume *guardOf(const G &c);
  std::string get(const std::string &k);
  void emit(int line, const std::string &what, const std::string &vs);
};

} // namespace verutop

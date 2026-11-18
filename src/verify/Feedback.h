#pragma once

#include "Gcl.h"

#include <map>

namespace verutop {

// ===========================================================================
// Counterexample traces.  Z3's model gives a value to every version; the
// trace follows the passive program along the path the model takes.
// ===========================================================================

struct SX {
  std::string atom;
  std::vector<SX> kids;
  bool list = false;
};
SX parseSX(const std::string &s, size_t &i);
// A model value as text: (- 1) is -1, (|Node| 1 |Leaf| |Leaf|) is
// Node(1, Leaf, Leaf).
std::string valueOf(const SX &x);

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

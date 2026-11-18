#pragma once

#include "Obligations.h"

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

class ASTFunction;
class ASTNode;

namespace verutop {

// Recursion through more than one function.  A call from f to g is on a
// cycle when g can also reach f; such a call needs a termination obligation
// just as a call from f to itself does.
class Cycles {
public:
  Cycles() = default;
  explicit Cycles(const std::set<std::pair<std::string, std::string>> &edges) {
    std::map<std::string, std::set<std::string>> next;
    for (auto &[a, b] : edges) next[a].insert(b);
    for (auto &[a, _] : next) {
      std::vector<std::string> work{a};
      while (!work.empty()) {
        auto x = work.back();
        work.pop_back();
        for (auto &y : next[x])
          if (reach[a].insert(y).second) work.push_back(y);
      }
    }
  }
  // f and g are different functions on one cycle.
  bool mutual(const std::string &f, const std::string &g) const {
    if (f == g) return false;
    auto a = reach.find(f), b = reach.find(g);
    return a != reach.end() && b != reach.end() && a->second.count(g) && b->second.count(f);
  }

private:
  std::map<std::string, std::set<std::string>> reach;
};
using CallEdges = std::set<std::pair<std::string, std::string>>;

// The obligation that a call from `from` to `to` on a cycle terminates:
// both need a measure, and `to`'s, at the call, must be smaller than
// `from`'s was on entry.  If either has none, the obligation is a missing
// clause and there is nothing to compare.
Obligation cycleObligation(ASTFunction *from, ASTFunction *to, ASTNode *call);

} // namespace verutop

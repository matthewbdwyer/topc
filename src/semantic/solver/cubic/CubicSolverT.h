#pragma once
// NO AST includes — Variable and Element are template parameters
#include <cassert>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <utility>
#include <vector>

template <typename Variable, typename Element>
class CubicSolverT;

template <typename Variable, typename Element>
class CubicSolverNode {
public:
  CubicSolverNode(int count) {
    bitvector.resize(count);
    conditionalConstraints.resize(count);
    for (int i = 0; i < count; i++) {
      bitvector[i] = false;
    }
    size = count;
  }

private:
  template <typename V, typename E> friend class CubicSolverT;
  std::set<std::shared_ptr<CubicSolverNode>> supsets;
  std::set<std::shared_ptr<CubicSolverNode>> subsets;
  std::vector<bool> bitvector;
  int size;
  std::vector<std::vector<std::pair<Variable, Variable>>> conditionalConstraints;
};

template <typename Variable, typename Element>
class CubicSolverT {
public:
  explicit CubicSolverT(std::vector<Element> elements) {
    for (int i = 0; i < static_cast<int>(elements.size()); i++) {
      fmapping[elements[i]] = i;
    }
  }

  void addElementofConstraint(Element elem, Variable var) {
    addEmptyVariableIfNecessary(var);
    dagmapping[var]->bitvector[fmapping[elem]] = true;
    propagateNodeChanges(dagmapping[var]);
  }

  void addConditionalConstraint(Element condition, Variable in,
                                Variable from, Variable to) {
    addEmptyVariableIfNecessary(in);
    addEmptyVariableIfNecessary(from);
    addEmptyVariableIfNecessary(to);
    dagmapping[in]->conditionalConstraints[fmapping[condition]].push_back(
        std::pair<Variable, Variable>(from, to));
    propagateNodeChanges(dagmapping[in]);
  }

  void addSubseteqConstraint(Variable from, Variable to) {
    addEmptyVariableIfNecessary(from);
    addEmptyVariableIfNecessary(to);
    if (dagmapping[from] == dagmapping[to]) {
      return;
    }
    dagmapping[from]->supsets.insert(dagmapping[to]);
    dagmapping[to]->subsets.insert(dagmapping[from]);
    killCyclesAt(dagmapping[from]);
    propagateNodeChanges(dagmapping[from]);
  }

  std::vector<Element> getElements(Variable var) {
    std::vector<Element> out;
    if (dagmapping.find(var) == dagmapping.end()) {
      return out;
    }
    for (auto pair : fmapping) {
      if (dagmapping[var]->bitvector[pair.second]) {
        out.push_back(pair.first);
      }
    }
    return out;
  }

private:
  using Node = CubicSolverNode<Variable, Element>;
  using NodePtr = std::shared_ptr<Node>;

  void activateConditionalConstraint(Variable from, Variable to) {
    if (dagmapping[from] == dagmapping[to]) {
      return;
    }
    dagmapping[from]->supsets.insert(dagmapping[to]);
    dagmapping[to]->subsets.insert(dagmapping[from]);
    killCyclesAt(dagmapping[from]);
    propagateNodeChanges(dagmapping[from]);
  }

  void addEmptyVariableIfNecessary(Variable node) {
    if (dagmapping.find(node) != dagmapping.end()) {
      return;
    }
    dagmapping[node] = std::make_shared<Node>(fmapping.size());
  }

  NodePtr killCyclesAt(NodePtr n) {
    bool collapsed;
    do {
      collapsed = false;
      for (NodePtr t : n->supsets) {
        auto path = findPath(t, n);
        if (path.size() == 0) {
          continue;
        }
        n = mergePath(path);
        collapsed = true;
        break;
      }
    } while (collapsed);
    return n;
  } // LCOV_EXCL_LINE

  std::vector<NodePtr> findPath(NodePtr source, NodePtr target) {
    std::map<NodePtr, int> distances;
    distances[source] = 0;
    std::queue<NodePtr> q;

    auto populateDistances = [&](NodePtr node) {
      assert(node != nullptr);
      for (NodePtr n : node->supsets) {
        if (distances.find(n) != distances.end()) {
          continue;
        }
        distances[n] = distances[node] + 1;
        q.push(n);
      }
    };

    populateDistances(source);

    while (!q.empty()) {
      NodePtr curr = q.front();
      populateDistances(curr);
      q.pop();
      if (distances.find(target) != distances.end()) {
        break;
      }
    }

    std::vector<NodePtr> out;

    if (distances.find(target) == distances.end()) {
      return out;
    } else {
      NodePtr curr = target;
      out.push_back(target);
      while (curr != source) {
        for (NodePtr n : curr->subsets) {
          if (distances.find(n) == distances.end()) {
            continue;
          }
          if (distances[n] == distances[curr] - 1) {
            curr = n;
          }
          break;
        }
        out.push_back(curr);
      }
      return out;
    }
  }

  NodePtr mergePath(std::vector<NodePtr> &path) {
    assert(path.size() != 0);
    while (true) {
      if (path.size() == 1) {
        return path[0];
      }
      NodePtr a = path.back();
      path.pop_back();
      NodePtr b = path.back();
      path.pop_back();
      path.push_back(mergeNodes(a, b));
    }
  }

  NodePtr mergeNodes(NodePtr n1, NodePtr n2) {
    for (auto &p : dagmapping) {
      if (p.second == n2) {
        p.second = n1;
      }
    }
    for (int i = 0; i < n1->size; i++) {
      n1->bitvector[i] = n1->bitvector[i] || n2->bitvector[i];
      for (auto a : n2->conditionalConstraints[i]) {
        n1->conditionalConstraints[i].push_back(a);
      }
    }
    for (auto a : n2->supsets) {
      if (n1 == a) {
        continue;
      }
      a->subsets.erase(a->subsets.find(n2));
      a->subsets.insert(n1);
      n1->supsets.insert(a);
    }
    for (auto a : n2->subsets) {
      if (n1 == a) {
        continue;
      }
      a->supsets.erase(a->supsets.find(n2));
      a->supsets.insert(n1);
      n1->subsets.insert(a);
    }
    if (n1->supsets.find(n2) != n1->supsets.end()) {
      n1->supsets.erase(n1->supsets.find(n2));
    }
    if (n1->subsets.find(n2) != n1->subsets.end()) {
      n1->subsets.erase(n1->subsets.find(n2));
    }
    return n1;
  }

  void propagateNodeChanges(NodePtr node) {
    for (int i = 0; i < node->size; i++) {
      if (node->bitvector[i]) {
        auto constraints = node->conditionalConstraints[i];
        node->conditionalConstraints[i].clear();
        for (auto pair : constraints) {
          activateConditionalConstraint(pair.first, pair.second);
        }
      }
    }
    for (NodePtr sups : node->supsets) {
      for (int i = 0; i < node->size; i++) {
        sups->bitvector[i] = sups->bitvector[i] || node->bitvector[i];
      }
      assert(sups != node);
      propagateNodeChanges(sups);
    }
  }

  std::map<Element, int> fmapping;
  std::map<Variable, NodePtr> dagmapping;
};

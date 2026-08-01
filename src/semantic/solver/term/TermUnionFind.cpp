#include "TermUnionFind.h"
#include "../../SemanticLogging.h"
#include <cassert>

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

std::shared_ptr<Term>
TermUnionFind::lookup_edge(const std::shared_ptr<Term> &t) const {
  for (auto const &edge : edges) {
    if (t->equals(*edge.first)) {
      return edge.first; // return the canonical stored pointer
    }
  }
  return nullptr;
}

std::shared_ptr<Term>
TermUnionFind::get_parent(const std::shared_ptr<Term> &t) {
  auto key = lookup_edge(t);
  assert(key != nullptr); // t must already be in edges
  return edges.at(key);
}

std::shared_ptr<Term> TermUnionFind::smart_insert(std::shared_ptr<Term> t) {
  auto existing = lookup_edge(t);
  if (existing) {
    return existing; // return the stored canonical pointer
  }
  // New term: create a self-loop (t is its own root)
  SEMANTIC_LOG(3, "union-find") << "insert term=" << t->toString();
  edges[t] = t;
  return t;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void TermUnionFind::add(std::shared_ptr<Term> term) { smart_insert(term); }

std::shared_ptr<Term> TermUnionFind::find(std::shared_ptr<Term> term) {
  term = smart_insert(term);

  auto parent = term;
  while (!parent->equals(*get_parent(parent))) {
    parent = get_parent(parent);
  }
  SEMANTIC_LOG(3, "union-find")
      << "find term=" << term->toString()
      << " representative=" << parent->toString();
  return parent;
}

void TermUnionFind::quick_union(std::shared_ptr<Term> t1,
                                std::shared_ptr<Term> t2) {
  t1 = smart_insert(t1);
  t2 = smart_insert(t2);

  auto t1_root = find(t1);
  auto t2_root = find(t2);
  SEMANTIC_LOG(3, "union-find")
      << "union from=" << t1_root->toString()
      << " to=" << t2_root->toString();

  // Make t1_root point to t2_root (t2_root becomes the canonical rep)
  auto key = lookup_edge(t1_root);
  if (key) {
    edges.erase(key);
    edges[t1_root] = t2_root;
  }
}

bool TermUnionFind::connected(std::shared_ptr<Term> t1,
                              std::shared_ptr<Term> t2) {
  return find(t1)->equals(*find(t2));
}

std::ostream &operator<<(std::ostream &os, const TermUnionFind &uf) {
  return uf.print(os);
}

std::ostream &TermUnionFind::print(std::ostream &out) const {
  out << "TermUnionFind edges {\n";
  for (auto const &edge : edges) {
    out << "  " << edge.first->toString() << " => " << edge.second->toString()
        << "\n";
  }
  out << "}";
  return out;
}

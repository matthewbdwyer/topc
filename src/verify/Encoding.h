#pragma once

#include "Term.h"

#include "TypeInference.h"

#include <functional>

namespace verutop {

// ===========================================================================
// Sorts.  int is Int; a sum type is an SMT datatype; a reference (owned or
// borrowed) has the sort of what it refers to.
// ===========================================================================

struct Encoding {
  TypeInference *ti = nullptr;
  std::map<std::string, std::string> ctorSort;                             // tag -> sort
  std::map<std::string, std::vector<std::pair<std::string, std::string>>> fields; // tag -> (selector, sort)
  std::map<std::string, std::string> selSort;                              // selector -> sort
  std::map<std::string, std::string> fnSort;                               // function -> result sort
  std::map<std::string, std::string> rankAxioms;                           // rank#D -> axiom
  std::map<std::string, std::string> rankChecks;                           // rank#D -> query
  std::string decls;                                                       // datatypes and rank functions

  static bool isDatatype(const std::string &s) { return s != "Int" && s != "Bool"; }
  std::string sortOf(const std::shared_ptr<TopType> &t) const;
  static bool isBorrow(const std::shared_ptr<TopType> &t);
  std::string sortOf(ASTDeclNode *d) const { return sortOf(ti->getInferredType(d)); }
  bool isBorrow(ASTDeclNode *d) const { return isBorrow(ti->getInferredType(d)); }

  // The result sort of a function, and the fields of a constructor.
  std::string resultSort(const std::string &fn) const;
  const std::vector<std::pair<std::string, std::string>> &fieldsOf(const std::string &tag) const;
  // The sort of a term; `lookup` gives the sort of a variable.
  std::string sortOfTerm(const T &t, const std::function<std::string(const std::string &)> &lookup) const;

  void build(ASTProgram *p, TypeInference *types);
  // rank#D(t), for a term t of datatype sort |D|.
  static T rank(T t, const std::string &sort);
};


} // namespace verutop

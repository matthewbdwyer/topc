#pragma once

#include "Query.h"
#include "Term.h"

#include "TypeInference.h"

#include "AST.h"

#include <functional>
#include <set>

namespace verutop {

// ===========================================================================
// How a TOP value becomes a logical one.  This is the whole encoding; no
// other module decides it.
//
// Heap data needs no heap model, because of ownership:
//
// - Sorts.  int is Int; a sum type is an SMT datatype; a reference, owned
//   or borrowed, has the sort of what it refers to.
// - References.  An owned box has one owner and no aliases, so a variable
//   of type own&T is represented by the value it points to: alloc E, *E
//   and &E are all the identity, and *p = E assigns p.  rootOf gives the
//   variable such an expression names.
// - Borrows.  A borrow lasts for a call, and only an int can be written
//   through one.  So a borrowed parameter the callee writes is an in-out
//   parameter (the caller's variable takes the value the callee's ensures
//   describes), and one it does not write leaves the caller's variable
//   alone, with no frame condition.  isWritten says which is which.
// - Sum values.  A value is a datatype value; case is a chain of
//   constructor tests, and its binders are selectors (matchCtor).  rank#D
//   measures a value's size, for `decreases`.
// - Machine integers.  At run time an int has 64 bits; in a formula it is
//   mathematical.  Under --overflow, code must keep its ints inRange.
//
// A verifier for a language without ownership (Dafny, Frama-C) needs, in
// place of this, a model of the heap and a frame condition on every call.
// ===========================================================================

struct Encoding {
  TypeInference *ti = nullptr;
  std::map<std::string, std::string> ctorSort;                             // tag -> sort
  std::map<std::string, std::vector<std::pair<std::string, std::string>>> fields; // tag -> (selector, sort)
  std::map<std::string, std::string> selSort;                              // selector -> sort
  std::map<std::string, std::string> fnSort;                               // function -> result sort
  std::map<std::string, Axiom> rankAxioms;                                  // rank#D -> its definition
  std::map<std::string, Query> rankChecks;                                  // rank#D -> the induction step
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

  // ---- references: the variable an lvalue or a borrowed argument names
  // (x, *x, **x, &x), and how many times it is dereferenced.
  static std::string rootOf(ASTExpr *e, int *derefs = nullptr);

  // ---- borrows: which borrowed parameters a function may write (through
  // *p = E, or by passing p on to a parameter that is written), and the
  // variables a statement may therefore change: its assignment targets, and
  // variables lent to a written parameter.
  bool isWritten(const std::string &f, int k) const { return written.count({f, k}); }
  std::set<std::string> assigned(ASTNode *s) const;

  // ---- sum values: a case arm as a condition on the scrutinee t, and the
  // values of its binders.
  T matchCtor(const std::string &tag, const std::vector<ASTPattern *> &subs, const T &t,
              std::vector<std::pair<ASTDeclNode *, T>> &binds) const;
  T matchPattern(ASTPattern *p, const T &t, std::vector<std::pair<ASTDeclNode *, T>> &binds) const;

  // ---- machine integers: t is a 64-bit value.  INT_MIN and INT_MAX are
  // defined in a query's prelude when --overflow is given.
  static T inRange(const T &t);
  // rank#D(t), for a term t of datatype sort |D|.
  static T rank(T t, const std::string &sort);

private:
  std::set<std::pair<std::string, int>> written; // (function, parameter index)
  void findWrites(ASTProgram *p);
};


} // namespace verutop

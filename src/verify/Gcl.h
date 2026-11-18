#pragma once

#include "Term.h"

#include <ostream>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace verutop {

// ===========================================================================
// Guarded commands.
// ===========================================================================

// Five commands and sequencing.  Each kind has only the fields it needs;
// a G is one of them.
struct G;

struct Assume {
  T p;
  int line = 0;
  std::string note;    // shown as a comment
  int asmId = -1;      // the tracked assumption it states, if any (Obligations.h)
  bool guard = false;  // the condition that picks a branch of a choice
  T src;               // a guard: the source condition; a passive assignment: the value
  std::string defines; // a passive assignment x := E, now x@k == E: the version x@k
};
struct Assert {
  T p;
  int id = -1; // obligation number
  int line = 0;
  std::string note;
  bool missing = false; // a clause the program lacks: it fails, but nothing
                        // after it assumes it
};
struct Assign {
  std::string x;
  T e;
  int line = 0;
  std::string note;
};
struct Havoc {
  std::vector<std::string> vars;
  std::vector<std::string> sorts; // the sort of each variable
  int line = 0;
  std::string note;
};
struct Seq {
  std::vector<G> kids;
};
struct Choice {
  std::vector<G> kids; // exactly two
  int line = 0;
};

struct G {
  std::variant<Seq, Assume, Assert, Assign, Havoc, Choice> v;

  G() = default; // the empty sequence
  template <class K, class = std::enable_if_t<!std::is_same_v<std::decay_t<K>, G>>>
  G(K &&k) : v(std::forward<K>(k)) {}

  // This command as a K, or null.
  template <class K> const K *as() const { return std::get_if<K>(&v); }
};

// match(g, [](const Assume &a) {...}, [](const Seq &s) {...}, ...): one
// function per kind, checked by the compiler to cover them all.
template <class... Fs> struct Overloaded : Fs... {
  using Fs::operator()...;
};
template <class... Fs> Overloaded(Fs...) -> Overloaded<Fs...>;
template <class... Fs> decltype(auto) match(const G &g, Fs &&...fs) {
  return std::visit(Overloaded{std::forward<Fs>(fs)...}, g.v);
}

Assume gAssume(T p, int line, std::string note, bool guard = false);
Assign gAssign(std::string x, T e, int line, std::string note = "");
// A sequence, with nested sequences flattened.
G gSeq(const std::vector<G> &ks);
G gChoice(G a, G b, int line);

// One primitive command as text lines (a formula may take several); none
// for a sequence or a choice.
std::vector<std::string> showCmd(const G &c, bool passive, int ind = 0);
void printG(const G &c, int ind, std::ostream &o, bool passive);

// The conditions that pick the branches of a program's choices, in order.
void guards(const G &c, std::vector<T> &out);

} // namespace verutop

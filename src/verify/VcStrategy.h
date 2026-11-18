#pragma once

#include "Lowering.h"
#include "Passivize.h"
#include "Query.h"

namespace verutop {

// ===========================================================================
// Verification conditions.  `target` is the obligation checked; every other
// obligation is assumed.  Target -2 checks them all (for --pvc and --pwp).
// ===========================================================================

// Whether this assertion is one the VC checks (and does not assume).
bool checked(const Assert &a, int target);

// wp by substitution, over guarded commands; then for the whole function,
// with the entry values of the formals.
T wp(const G &c, const T &Q, int target);
T wpFunction(const Proc &p, int target);

// The VC of a passive command needs no substitution.  For each command:
// ok (its assertions hold) and post (what holds when it finishes).
//   ok(S1; S2) = ok1 && (post1 ==> ok2)      post(S1; S2) = post1 && post2
//   ok(S1 [] S2) = ok1 && ok2                post(S1 [] S2) = post1 || post2
// Each post is named with a define-fun, so it is written once and the VC
// grows linearly with the program.
struct PassiveVC {
  std::vector<std::pair<std::string, T>> defs;
  T ok;
};
PassiveVC passiveVc(const G &prog, int target);

// A verification condition, by either method, in the form a query needs:
// `formula` is valid exactly when obligation `target` holds (given the
// others).  It may mention the constants and, in order, the definitions.
struct Vc {
  std::vector<Decl> consts;
  std::vector<std::pair<std::string, T>> defs;
  T formula;
  std::vector<std::string> entry; // the constants that hold the arguments
};

// Both strategies compute the weakest precondition of the function's body.
// They differ in the program they compute it from and in how the formula is
// written, and so in its size: every obligation gets the same verdict from
// both.
class VcStrategy {
public:
  virtual ~VcStrategy() = default;
  virtual std::string name() const = 0;
  virtual Vc vc(const Proc &proc, const Passive &passive, int target) const = 0;
};
// wp by substitution, over the guarded commands as lowered: an assignment
// substitutes into everything after it, and the rest of the function is
// repeated under both branches of every choice.  The formula can be
// exponential in the number of branches.
class SubstVc : public VcStrategy {
public:
  std::string name() const override { return "wp"; }
  Vc vc(const Proc &proc, const Passive &passive, int target) const override;
};
// wp of the passive commands.  With nothing assigned there is nothing to
// substitute, and for a passive command S
//     wp(S, Q)  =  ok(S) && (post(S) ==> Q)
// so Q, the rest of the function, is written once, and each post is given
// a name.  The formula is linear in the program (Flanagan and Saxe).  The
// VC is ok(body), which is wp(body, true).
class PassiveVc : public VcStrategy {
public:
  std::string name() const override { return "passive"; }
  Vc vc(const Proc &proc, const Passive &passive, int target) const override;
};
// The passive VC of a passive program that has been transformed (flagged).
Vc passiveVcOf(const Passive &passive, const G &prog, int target);

} // namespace verutop

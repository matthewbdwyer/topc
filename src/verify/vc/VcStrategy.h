#pragma once

#include "Proc.h"
#include "Passivize.h"
#include "Query.h"
#include "VCGen.h"

#include <memory>

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

// wp of a passive program, by the same rules as wp above.  Nothing is
// assigned, so nothing is substituted; a havoc there only records that new
// versions start, and is no command.  Q is still copied under both
// branches of every choice.
T wpPassive(const G &passiveProg, int target);

// Three ways to compute the weakest precondition of a function's body.
// Every obligation gets the same verdict from all of them; they differ in
// the program they start from and in where the rest of the function, Q,
// is written, and so in size:
//
//                    program       Q under a choice      size in n ifs
//   SubstVc          as lowered    copied, substituted   exponential
//   PassiveCopyVc    passive       copied                exponential
//   PassiveVc        passive       written once          linear
//
// The middle one exists to show that versioning alone does not make the VC
// small.  What it does is make Q the same formula under both branches, so
// that it can be written once: (A ==> Q) && (B ==> Q) is (A || B) ==> Q.
class VcStrategy {
public:
  virtual ~VcStrategy() = default;
  // As the report names it: "VC (wp by substitution)".
  virtual std::string name() const = 0;
  virtual Vc vc(const Proc &proc, const Passive &passive, int target) const = 0;
};
// wp over the guarded commands as lowered: an assignment substitutes into
// everything after it.
class SubstVc : public VcStrategy {
public:
  std::string name() const override { return "wp by substitution"; }
  Vc vc(const Proc &proc, const Passive &passive, int target) const override;
};
// wpPassive: no substitution, Q copied.
class PassiveCopyVc : public VcStrategy {
public:
  std::string name() const override { return "wp, passive, rest copied"; }
  Vc vc(const Proc &proc, const Passive &passive, int target) const override;
};
// For a passive command S
//     wp(S, Q)  =  ok(S) && (post(S) ==> Q)
// so Q is written once, and each post is given a name (Flanagan and Saxe).
// The VC is ok(body), which is wp(body, true).
class PassiveVc : public VcStrategy {
public:
  std::string name() const override { return "wp, passive"; }
  Vc vc(const Proc &proc, const Passive &passive, int target) const override;
};
std::unique_ptr<VcStrategy> makeStrategy(VCGen::VcMode mode);

// The passive VC of a passive program that has been transformed (flagged).
Vc passiveVcOf(const Passive &passive, const G &prog, int target);

} // namespace verutop

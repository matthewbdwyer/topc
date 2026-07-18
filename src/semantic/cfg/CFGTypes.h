#pragma once

#include <cstddef>
#include <string>

using BlockId = std::size_t;

enum class CFGEdgeKind {
  Fallthrough,
  TrueBranch,
  FalseBranch,
  CaseArm,
  ReturnToExit,
};

struct CFGEdge {
  BlockId target;
  CFGEdgeKind kind;
  std::string label;
};

enum class CFGTerminatorKind {
  Fallthrough,
  If,
  While,
  Case,
  Return,
};

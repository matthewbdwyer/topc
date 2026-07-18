#pragma once

#include <iosfwd>

class ControlFlowGraph;

class CFGRenderer {
public:
  static void renderAscii(const ControlFlowGraph &cfg, std::ostream &os);
  static void renderDot(const ControlFlowGraph &cfg, std::ostream &os);
};

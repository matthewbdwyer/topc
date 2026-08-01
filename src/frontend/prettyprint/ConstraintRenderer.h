#pragma once

#include <iosfwd>
#include <string>
#include <vector>

struct ConstraintRecord {
  std::string label;
  int line;
  int column;
  std::string text;
};

class ConstraintRenderer {
public:
  static std::string formatSpan(int line, int column);

  static void renderSection(const std::string &section,
                            const std::vector<ConstraintRecord> &records,
                            std::ostream &os);
};

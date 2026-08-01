#include "ConstraintRenderer.h"

#include <algorithm>
#include <ostream>
#include <tuple>

std::string ConstraintRenderer::formatSpan(int line, int column) {
  if (line <= 0 || column <= 0) {
    return "-";
  }
  return std::to_string(line) + ":" + std::to_string(column);
}

void ConstraintRenderer::renderSection(
    const std::string &section, const std::vector<ConstraintRecord> &records,
    std::ostream &os) {
  os << "\n[" << section << "]\n";

  std::vector<ConstraintRecord> ordered = records;
  std::sort(ordered.begin(), ordered.end(), [](const auto &a, const auto &b) {
    return std::tie(a.line, a.column, a.label, a.text) <
           std::tie(b.line, b.column, b.label, b.text);
  });

  if (ordered.empty()) {
    os << "  (none)\n";
    return;
  }

  for (const auto &record : ordered) {
    os << "  [" << record.label << "] "
       << formatSpan(record.line, record.column) << " " << record.text
       << "\n";
  }
}

#pragma once

#include "core/VerificationTraceRecorder.h"
#include <sstream>
#include <string>

namespace NeuroForge {
namespace Visualization {

/**
 * @brief Phase 20a: Graphviz DOT output for verification trees
 *
 * Generates DOT format for debugging, papers, and slides.
 * Usage: dot -Tpng verification.dot -o verification.png
 */
class VerificationGraphviz {
public:
  /**
   * @brief Convert trace to DOT format
   */
  static std::string toDOT(const Core::VerificationTraceRecorder &recorder) {
    std::ostringstream ss;
    ss << "digraph VerificationTree {\n";
    ss << "    node [shape=box style=rounded fontname=\"Helvetica\"];\n";
    ss << "    rankdir=TB;\n\n";

    const auto &trace = recorder.getTrace();

    // Generate nodes
    for (const auto &[id, node] : trace) {
      std::string color = getStatusColor(node.status);

      ss << "    " << id << " [label=\"" << escapeLabel(node.subject) << " "
         << escapeLabel(node.predicate) << " " << escapeLabel(node.object)
         << "\\n"
         << "Depth:" << node.depth << " "
         << "ROI:" << std::fixed << std::setprecision(2) << node.roi_score
         << "\\n"
         << "Status:" << Core::traceStatusToString(node.status) << "\""
         << " color=" << color << " penwidth=2];\n";
    }

    ss << "\n";

    // Generate edges
    for (const auto &[id, node] : trace) {
      for (int child_id : node.children) {
        ss << "    " << id << " -> " << child_id << ";\n";
      }
    }

    ss << "}\n";
    return ss.str();
  }

private:
  static std::string getStatusColor(Core::TraceStatus status) {
    switch (status) {
    case Core::TraceStatus::PENDING:
      return "gray";
    case Core::TraceStatus::FETCHED:
      return "blue";
    case Core::TraceStatus::PROMOTED:
      return "green";
    case Core::TraceStatus::FAILED:
      return "red";
    case Core::TraceStatus::ABANDONED:
      return "orange";
    default:
      return "black";
    }
  }

  static std::string escapeLabel(const std::string &s) {
    std::string result;
    for (char c : s) {
      if (c == '"')
        result += "\\\"";
      else if (c == '\\')
        result += "\\\\";
      else
        result += c;
    }
    return result;
  }
};

} // namespace Visualization
} // namespace NeuroForge

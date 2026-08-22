#pragma once

#include "core/VerificationTraceRecorder.h"
#include <iomanip>
#include <sstream>
#include <string>


namespace NeuroForge {
namespace Visualization {

/**
 * @brief Phase 20a: JSON export for verification trees
 *
 * Generates JSON for UI, web dashboards, and analytics.
 * Compatible with D3.js, Cytoscape, React visualization libraries.
 */
class VerificationJSON {
public:
  /**
   * @brief Export trace to JSON string
   */
  static std::string
  exportTree(const Core::VerificationTraceRecorder &recorder) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);
    ss << "{\n";
    ss << "  \"nodes\": [\n";

    const auto &trace = recorder.getTrace();
    bool first = true;

    for (const auto &[id, node] : trace) {
      if (!first)
        ss << ",\n";
      first = false;

      ss << "    {\n";
      ss << "      \"id\": " << id << ",\n";
      ss << "      \"parent\": " << node.parent_frame_id << ",\n";
      ss << "      \"fact\": \"" << escapeJSON(node.subject) << " "
         << escapeJSON(node.predicate) << " " << escapeJSON(node.object)
         << "\",\n";
      ss << "      \"depth\": " << node.depth << ",\n";
      ss << "      \"status\": \"" << Core::traceStatusToString(node.status)
         << "\",\n";
      ss << "      \"source\": \"" << escapeJSON(node.source_url) << "\",\n";
      ss << "      \"domain_class\": \"" << escapeJSON(node.source_domain_class)
         << "\",\n";
      ss << "      \"value\": " << node.epistemic_value << ",\n";
      ss << "      \"cost\": " << node.verification_cost << ",\n";
      ss << "      \"roi\": " << node.roi_score << "\n";
      ss << "    }";
    }

    ss << "\n  ]\n";
    ss << "}\n";
    return ss.str();
  }

private:
  static std::string escapeJSON(const std::string &s) {
    std::string result;
    for (char c : s) {
      if (c == '"')
        result += "\\\"";
      else if (c == '\\')
        result += "\\\\";
      else if (c == '\n')
        result += "\\n";
      else if (c == '\r')
        result += "\\r";
      else if (c == '\t')
        result += "\\t";
      else
        result += c;
    }
    return result;
  }
};

} // namespace Visualization
} // namespace NeuroForge

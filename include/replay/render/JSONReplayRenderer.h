#pragma once

#include "replay/ReplayEvent.h"
#include <sstream>
#include <string>
#include <vector>


namespace NeuroForge {
namespace Replay {

/**
 * @brief Phase 21a: JSON renderer for replay events
 *
 * Compatible with D3.js, React, and other web visualization tools.
 */
class JSONReplayRenderer {
public:
  /**
   * @brief Render events to JSON string
   */
  static std::string render(const std::vector<ReplayEvent> &events) {
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"events\": [\n";

    bool first = true;
    for (const auto &e : events) {
      if (!first)
        ss << ",\n";
      first = false;

      ss << "    {\n";
      ss << "      \"frame_id\": " << e.frame_id << ",\n";
      ss << "      \"type\": \"" << ReplayEvent::typeToString(e.type)
         << "\",\n";
      ss << "      \"summary\": \"" << escapeJSON(e.summary) << "\",\n";
      ss << "      \"timestamp_ms\": " << e.timestamp_ms << "\n";
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

} // namespace Replay
} // namespace NeuroForge

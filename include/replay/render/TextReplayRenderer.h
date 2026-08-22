#pragma once

#include "replay/ReplayEvent.h"
#include <iomanip>
#include <iostream>
#include <vector>


namespace NeuroForge {
namespace Replay {

/**
 * @brief Phase 21a: CLI text renderer for replay events
 */
class TextReplayRenderer {
public:
  /**
   * @brief Render events to stdout
   */
  static void render(const std::vector<ReplayEvent> &events,
                     std::ostream &out = std::cout) {
    int current_frame = -1;

    for (const auto &e : events) {
      // Frame separator
      if (e.frame_id != current_frame) {
        if (current_frame != -1) {
          out << "\n";
        }
        out << "=== Frame " << e.frame_id << " ===\n";
        current_frame = e.frame_id;
      }

      // Event line
      out << "  [" << std::setw(7) << ReplayEvent::typeToString(e.type) << "] "
          << e.summary << "\n";
    }
  }

  /**
   * @brief Render to string
   */
  static std::string renderToString(const std::vector<ReplayEvent> &events) {
    std::ostringstream ss;
    render(events, ss);
    return ss.str();
  }
};

} // namespace Replay
} // namespace NeuroForge

#pragma once

#include "actuation/ReplayFrame.h"
#include <vector>

namespace NeuroForge {
namespace Replay {

/**
 * @brief Phase 21a: Abstract interface for replay sources
 *
 * Read-only access to recorded cognition frames.
 */
class ReplaySource {
public:
  virtual ~ReplaySource() = default;

  /**
   * @brief Load all replay frames from this source
   */
  virtual std::vector<Actuation::ReplayFrame> load() = 0;

  /**
   * @brief Get the number of available frames
   */
  virtual std::size_t count() const = 0;
};

} // namespace Replay
} // namespace NeuroForge

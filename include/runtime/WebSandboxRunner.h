#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>


namespace NeuroForge {
namespace Core {
class MemoryDB;
}
namespace Perception {
class WorldModelCortex;
}
namespace Runtime {

struct WebSandboxConfig {
  std::string start_url =
      "https://en.wikipedia.org/wiki/Special:Random"; // Good source of semantic
                                                      // drift
  int window_width = 1280;
  int window_height = 800;
  bool enable_vision = false; // Capture screenshots? (expensive)

  std::string log_csv_path = "web_log.csv";
};

struct WebSandboxSinks {
  std::function<void(const std::string &)> emit_json_line;
};

// The Web Observer Loop
// Reads from WebSandbox, Feeds WorldModel, Logs Bias Activation.
int runWebSandbox(NeuroForge::Perception::WorldModelCortex &world_model_cortex,
                  NeuroForge::Core::MemoryDB *memdb, std::uint64_t memdb_run_id,
                  bool log_json, const WebSandboxSinks &sinks,
                  const WebSandboxConfig &cfg);

} // namespace Runtime
} // namespace NeuroForge

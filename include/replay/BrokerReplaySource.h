#pragma once

#include "actuation/ActionBroker.h"
#include "replay/ReplaySource.h"


namespace NeuroForge {
namespace Replay {

/**
 * @brief Phase 21a: Replay source from ActionBroker's replay log
 */
class BrokerReplaySource : public ReplaySource {
public:
  explicit BrokerReplaySource(const Actuation::ActionBroker &broker)
      : broker_(broker) {}

  std::vector<Actuation::ReplayFrame> load() override {
    return broker_.getReplayLog();
  }

  std::size_t count() const override { return broker_.getReplayLog().size(); }

private:
  const Actuation::ActionBroker &broker_;
};

} // namespace Replay
} // namespace NeuroForge

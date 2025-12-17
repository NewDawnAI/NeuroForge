#include "core/ActionFilter.h"
#include "core/Logger.h"

namespace NeuroForge {
namespace Core {

ActionFilterDecision ActionFilter_check(ActionKind kind,
                                        bool sandbox_actions_enable,
                                        const std::string& phase15_decision,
                                        const std::string& phase13_decision,
                                        int simulate_blocked) {
    (void)kind;
    (void)phase13_decision;
    // simulation flag is for metrics only; do not gate real actions based on it
    if (!sandbox_actions_enable) {
        NF_LOG_INFO("ActionFilter", "blocked: no_web_actions");
        return {false, std::string("no_web_actions")};
    }
    if (phase15_decision == std::string("deny")) {
        NF_LOG_INFO("ActionFilter", "blocked: phase15_deny");
        return {false, std::string("phase15_deny")};
    }
    if (phase13_decision == std::string("freeze")) {
        NF_LOG_INFO("ActionFilter", "blocked: phase13_freeze");
        return {false, std::string("phase13_freeze")};
    }
    NF_LOG_INFO("ActionFilter", "allowed");
    return {true, std::string("ok")};
}

} // namespace Core
} // namespace NeuroForge

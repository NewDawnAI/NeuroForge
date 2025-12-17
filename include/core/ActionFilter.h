#pragma once

#include <string>

namespace NeuroForge {
namespace Core {

enum class ActionKind {
    TypeText,
    KeyPress,
    ScrollUp,
    ScrollDown,
    Click,
    CursorMove
};

struct ActionFilterDecision {
    bool allow{true};
    std::string reason{"ok"};
};

ActionFilterDecision ActionFilter_check(ActionKind kind,
                                        bool sandbox_actions_enable,
                                        const std::string& phase15_decision,
                                        const std::string& phase13_decision,
                                        int simulate_blocked);

} // namespace Core
} // namespace NeuroForge


#pragma once

#include <string>
#include <cstdint>

// WebSandbox provides a dedicated window that the agent can safely interact with.
// On Windows, it optionally hosts an Edge WebView2 instance for real browsing.
// The class exposes simple controls (navigate, scroll, click, type) and
// returns client-area bounds both in local (0,0) and absolute screen coordinates.

namespace NeuroForge {
namespace Sandbox {

struct SandboxRect {
    int x{0};
    int y{0};
    int w{0};
    int h{0};
};

class WebSandbox {
public:
    WebSandbox();
    ~WebSandbox();

    bool create(int width, int height, const std::string& title);
    bool navigate(const std::string& url);
    void flushPendingNavigation();
    bool scroll(int delta);
    bool click(int cx, int cy);
    bool typeText(const std::string& text);
    bool focus();
    bool sendKey(unsigned int vk);
    SandboxRect bounds() const;
    SandboxRect screenBounds() const;
    bool isOpen() const;
    bool waitUntilReady(int timeout_ms);

    void updateBoundsFromClient();

private:
    void* hwnd_{nullptr};
    void* webview_env_{nullptr};
    void* webview_controller_{nullptr};
    void* webview_window_{nullptr};
    void* webview_nav_completed_handler_{nullptr};
    void* webview_nav_starting_handler_{nullptr};
    void* webview_process_failed_handler_{nullptr};
    std::string pending_url_{};
    std::wstring user_data_folder_{};
    SandboxRect rect_{};
    int bounds_update_count_{0};
    bool env_ready_{false};
    bool controller_ready_{false};
    bool navigation_requested_{false};
    bool navigation_started_{false};
    bool navigation_completed_{false};
    bool webview_process_failed_{false};
    bool com_initialized_{false};
    int pending_nav_attempts_{0};
    std::uint32_t pending_nav_start_tick_{0};
    std::uint32_t pending_nav_last_attempt_tick_{0};
};

} // namespace Sandbox
} // namespace NeuroForge

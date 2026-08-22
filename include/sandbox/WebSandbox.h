#pragma once

#include <utility>
#include <vector>

#include "sandbox/ScreenshotCapture.h"
#include <cstdint>
#include <string>

// WebSandbox provides a dedicated window that the agent can safely interact
// with. On Windows, it optionally hosts an Edge WebView2 instance for real
// browsing. The class exposes simple controls (navigate, scroll, click, type)
// and returns client-area bounds both in local (0,0) and absolute screen
// coordinates.

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

  bool create(int width, int height, const std::string &title);
  bool navigate(const std::string &url);
  void flushPendingNavigation();
  bool scroll(int delta);
  bool click(int cx, int cy);
  bool typeText(const std::string &text);
  bool focus();
  bool sendKey(unsigned int vk);
  SandboxRect bounds() const;
  SandboxRect screenBounds() const;
  bool isOpen() const;
  bool waitUntilReady(int timeout_ms);

  // DOM Text Extraction (WebView2 only)
  // Returns the extracted text content from the current page
  std::string getPageText() const;

  // Visual Browsing: Advanced Interaction
  // Scroll to specific element via CSS selector
  bool scrollToElement(const std::string &selector);

  // Get current scroll position {x, y}
  std::pair<int, int> getScrollPosition();

  // Get element bounding box {x, y, w, h}
  SandboxRect getElementBounds(const std::string &selector);

  // Click element by selector (finds bounds, then clicks center)
  bool clickElement(const std::string &selector);

  // Get the page title

  // Get the page title
  std::string getPageTitle() const;

  // Extract text content from current page (async, call after navigation
  // completes) This triggers JavaScript execution to get
  // document.body.innerText
  bool extractPageContent();

  // Check if text extraction is complete
  bool isExtractionComplete() const;

  // Get current URL
  std::string getCurrentUrl() const;

  void updateBoundsFromClient();

  // Process pending window messages (must be called periodically on the
  // creation thread)
  void poll();

  // Visual Browsing: Capture screenshot
  ScreenshotInfo captureScreenshot(const ScreenshotConfig &config = {});

private:
  void *hwnd_{nullptr};
  void *webview_env_{nullptr};
  void *webview_controller_{nullptr};
  void *webview_window_{nullptr};
  void *webview_nav_completed_handler_{nullptr};
  void *webview_nav_starting_handler_{nullptr};
  void *webview_process_failed_handler_{nullptr};
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

  // DOM extraction
  std::string extracted_text_{};
  std::string page_title_{};
  std::string current_url_{};
  bool extraction_complete_{false};
  bool extraction_in_progress_{false};

  ScreenshotCapture screenshot_capture_;

  // Helper for synchronous script execution (Visual Browsing)
  std::string executeScriptBlocking(const std::wstring &script);
};

} // namespace Sandbox
} // namespace NeuroForge

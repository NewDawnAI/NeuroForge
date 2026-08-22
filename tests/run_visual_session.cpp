/**
 * @file run_visual_session.cpp
 * @brief Visual Browsing Session Runner
 *
 * Demonstrates the "See, Click, Scroll" capabilities.
 * Uses WebSandbox for rendering and BrowserVision for perception.
 * curiosity-driven navigation decisions are made by CuriosityNavigator.
 */

#include "core/LanguageSystem.h"
#include "navigation/CuriosityNavigator.h"
#include "sandbox/WebSandbox.h"
#include "vision/BrowserVision.h"
#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>


using namespace NeuroForge;

static std::atomic<bool> g_running{true};

void signalHandler(int signal) { g_running = false; }

int main(int argc, char *argv[]) {
  signal(SIGINT, signalHandler);

  std::cout << "\n==========================================" << std::endl;
  std::cout << " NeuroForge VISUAL BROWSING Session       " << std::endl;
  std::cout << " See, Click, Scroll Demo                  " << std::endl;
  std::cout << "==========================================" << std::endl;

  // Initialize Language System (minimal needed for Navigator)
  Core::LanguageSystem::Config lang_config;
  auto language_system = std::make_unique<Core::LanguageSystem>(lang_config);
  language_system->initialize();

  // Initialize Curiosity Navigator
  Navigation::NavigatorConfig nav_config;
  nav_config.rate_limit_ms = 2000;
  nav_config.curiosity_threshold = 0.2f; // Low threshold for demo
  auto navigator = std::make_unique<Navigation::CuriosityNavigator>(
      language_system.get(), nullptr, nav_config);
  navigator->start();

  // Initialize WebSandbox
  auto sandbox = std::make_unique<Sandbox::WebSandbox>();
  if (!sandbox->create(1280, 800, "NeuroForge Visual Agent")) {
    std::cerr << "Failed to create WebSandbox window.\n";
    return 1;
  }

  // Initialize Vision
  Vision::BrowserVision vision;
  if (!vision.isAvailable()) {
    std::cout << "[Warn] BrowserVision backend (OpenCV) not fully available. "
                 "Functionality may be limited.\n";
  }

  // Start URL
  std::string start_url =
      "https://en.wikipedia.org/wiki/Artificial_intelligence";
  if (argc > 1)
    start_url = argv[1];

  std::cout << "[Visual] Navigating to: " << start_url << "\n";
  sandbox->navigate(start_url);

  // Add initial goal
  navigator->addGoal("visual exploration", 1.0f, {start_url});

  while (g_running && sandbox->isOpen()) {
    sandbox->poll(); // Process window events

    // Simple state machine
    static auto last_action_time = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();

    if (std::chrono::duration_cast<std::chrono::milliseconds>(now -
                                                              last_action_time)
            .count() < 3000) {
      std::this_thread::sleep_for(std::chrono::milliseconds(50));
      continue;
    }

    // 1. Capture State
    auto screenshot = sandbox->captureScreenshot();
    if (screenshot.width == 0)
      continue;

    // 2. Visual Perception
    std::cout << "[Visual] Analyzing frame (" << screenshot.width << "x"
              << screenshot.height << ")...\n";
    auto analysis = vision.analyze(screenshot);
    std::cout << "[Visual] Found " << analysis.elements.size()
              << " visual elements.\n";

    // 3. Update Navigator
    navigator->processVisuals(analysis);

    // Also integrate DOM links if we wanted, but let's focus on visual for now.
    // In a real loop, we'd do both.

    // 4. Decide Action
    auto action = navigator->getNextAction();

    if (action.type == Navigation::NavigationAction::Type::Wait) {
      std::cout << "[Action] Waiting...\n";
    } else if (action.type == Navigation::NavigationAction::Type::Navigate) {
      // Here, the navigator might give a URL (from a link) OR a visual target

      // Check if target looks like our visual encoded string "click:x,y"
      if (action.target.rfind("click:", 0) == 0) {
        // Visual Click
        std::string coords = action.target.substr(6);
        auto comma = coords.find(',');
        if (comma != std::string::npos) {
          int cx = std::stoi(coords.substr(0, comma));
          int cy = std::stoi(coords.substr(comma + 1));

          std::cout << "[Action] Visual CLICK at (" << cx << "," << cy << ") - "
                    << action.rationale << "\n";
          sandbox->click(cx, cy);
        }
      } else {
        // Standard Navigation
        std::cout << "[Action] Navigate to: " << action.target << "\n";
        sandbox->navigate(action.target);
      }
    } else if (action.type == Navigation::NavigationAction::Type::Scroll) {
      std::cout << "[Action] Scrolling...\n";
      // sandbox->scroll(...) // Need to implement scroll API in sandbox header
      // to be used here For now, let's just use executeScript or similar if
      // exposed, or add to sandbox. We added scrollToElement, maybe we need
      // simple scroll(dx, dy).
    }

    last_action_time = now;
  }

  return 0;
}

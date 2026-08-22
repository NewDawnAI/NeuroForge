#pragma once

#include "sandbox/ScreenshotCapture.h"
#include <string>
#include <vector>

namespace NeuroForge {
namespace Vision {

struct VisualElement {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
  std::string text; // OCR result or recognized label
  std::string type; // "button", "link", "text", "input"
  float confidence = 0.0f;
};

struct VisualPageAnalysis {
  std::vector<VisualElement> elements;
  bool has_scrollbars = false;
  float meaningful_content_ratio = 0.0f;
};

class BrowserVision {
public:
  BrowserVision();
  ~BrowserVision();

  // Analyze screenshot to find interactive elements and text
  VisualPageAnalysis analyze(const Sandbox::ScreenshotInfo &screenshot);

  // Check if OpenCV/OCR backend is available
  bool isAvailable() const;

private:
  // Pimpl idiom or implementation details
  struct Impl;
  Impl *pimpl_;
};

} // namespace Vision
} // namespace NeuroForge

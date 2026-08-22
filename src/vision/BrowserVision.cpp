#include "vision/BrowserVision.h"
#include "sandbox/ScreenshotCapture.h"
#include <iostream>

#ifdef NF_HAVE_OPENCV
#include <opencv2/opencv.hpp>
// If we had Tesseract or DNN text detection
#endif

namespace NeuroForge {
namespace Vision {

struct BrowserVision::Impl {
  // OpenCV objects here
};

BrowserVision::BrowserVision() : pimpl_(new Impl()) {}

BrowserVision::~BrowserVision() { delete pimpl_; }

bool BrowserVision::isAvailable() const {
#ifdef NF_HAVE_OPENCV
  return true;
#else
  return false;
#endif
}

VisualPageAnalysis
BrowserVision::analyze(const Sandbox::ScreenshotInfo &screenshot) {
  VisualPageAnalysis result;

#ifdef NF_HAVE_OPENCV
  if (screenshot.filepath.empty())
    return result;

  cv::Mat img = cv::imread(screenshot.filepath);
  if (img.empty())
    return result;

  // Basic Layout Analysis (Edge detection + Contours to find boxes)
  cv::Mat gray, edges;
  cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
  cv::Canny(gray, edges, 50, 150);

  std::vector<std::vector<cv::Point>> contours;
  cv::findContours(edges, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

  for (const auto &cnt : contours) {
    cv::Rect r = cv::boundingRect(cnt);
    if (r.width > 20 && r.height > 10) {
      VisualElement el;
      el.x = r.x;
      el.y = r.y;
      el.width = r.width;
      el.height = r.height;
      el.type = "unknown";
      el.confidence = 0.5f;
      result.elements.push_back(el);
    }
  }

  // To do real layout understanding we need more processing
  // But this connects the "Pixel" world to "Data" world.
#endif

  return result;
}

} // namespace Vision
} // namespace NeuroForge

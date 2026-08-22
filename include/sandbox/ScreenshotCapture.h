#pragma once

/**
 * @file ScreenshotCapture.h
 * @brief Capture WebView2 viewport as images
 *
 * Part of Visual Browsing capabilities.
 * Enables human-like perception of web pages.
 */

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace NeuroForge {
namespace Sandbox {

/**
 * @brief Screenshot format
 */
enum class ImageFormat { PNG, JPEG, BMP };

/**
 * @brief Screenshot metadata
 */
struct ScreenshotInfo {
  std::string filepath;
  int width = 0;
  int height = 0;
  std::uint64_t timestamp_ms = 0;
  std::string url;
  ImageFormat format = ImageFormat::PNG;
  std::size_t file_size = 0;
};

/**
 * @brief Screenshot capture configuration
 */
struct ScreenshotConfig {
  ImageFormat format = ImageFormat::PNG;
  int quality = 90; // For JPEG
  bool include_scrollbar = false;
  bool full_page = false; // Capture full page vs visible viewport
  std::string output_dir = "screenshots";
};

/**
 * @brief Screenshot capture for WebView2
 */
class ScreenshotCapture {
public:
  ScreenshotCapture() = default;

  /**
   * @brief Set configuration
   */
  void setConfig(const ScreenshotConfig &config) { config_ = config; }

  /**
   * @brief Capture viewport screenshot (Windows GDI method)
   *
   * @param hwnd Window handle of WebView2
   * @param url Current URL for metadata
   * @return ScreenshotInfo with filepath and metadata
   */
  ScreenshotInfo captureViewport(void *hwnd, const std::string &url) {
    ScreenshotInfo info;
    info.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch())
                            .count();
    info.url = url;
    info.format = config_.format;

#ifdef _WIN32
    HWND window = static_cast<HWND>(hwnd);
    if (!window) {
      return info;
    }

    // Get window dimensions
    RECT rect;
    GetClientRect(window, &rect);
    info.width = rect.right - rect.left;
    info.height = rect.bottom - rect.top;

    if (info.width <= 0 || info.height <= 0) {
      return info;
    }

    // Create device contexts
    HDC hdcWindow = GetDC(window);
    HDC hdcMemDC = CreateCompatibleDC(hdcWindow);

    // Create bitmap
    HBITMAP hbmScreen =
        CreateCompatibleBitmap(hdcWindow, info.width, info.height);
    SelectObject(hdcMemDC, hbmScreen);

    // Copy screen to bitmap
    BitBlt(hdcMemDC, 0, 0, info.width, info.height, hdcWindow, 0, 0, SRCCOPY);

    // Ensure directory exists
    try {
      std::filesystem::create_directories(config_.output_dir);
    } catch (...) {
    }

    // Generate filename
    std::string filename = config_.output_dir + "/screenshot_" +
                           std::to_string(info.timestamp_ms) +
                           getExtension(config_.format);

    // Save bitmap to file (BMP format for simplicity)
    saveBitmap(hbmScreen, info.width, info.height, filename);
    info.filepath = filename;

    // Get file size
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (file) {
      info.file_size = static_cast<std::size_t>(file.tellg());
    }

    // Cleanup
    DeleteObject(hbmScreen);
    DeleteDC(hdcMemDC);
    ReleaseDC(window, hdcWindow);

    screenshots_.push_back(info);
#endif

    return info;
  }

  /**
   * @brief Get all captured screenshots
   */
  const std::vector<ScreenshotInfo> &getScreenshots() const {
    return screenshots_;
  }

  /**
   * @brief Get screenshot count
   */
  std::size_t count() const { return screenshots_.size(); }

  /**
   * @brief Clear screenshot history
   */
  void clear() { screenshots_.clear(); }

private:
  ScreenshotConfig config_;
  std::vector<ScreenshotInfo> screenshots_;

  std::string getExtension(ImageFormat fmt) const {
    switch (fmt) {
    case ImageFormat::PNG:
      return ".png";
    case ImageFormat::JPEG:
      return ".jpg";
    case ImageFormat::BMP:
      return ".bmp";
    default:
      return ".bmp";
    }
  }

#ifdef _WIN32
  void saveBitmap(HBITMAP hBitmap, int width, int height,
                  const std::string &filename) {
    // Get bitmap info
    BITMAP bmp;
    GetObject(hBitmap, sizeof(BITMAP), &bmp);

    BITMAPFILEHEADER bmfHeader;
    BITMAPINFOHEADER bi;

    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = width;
    bi.biHeight = -height; // Negative for top-down
    bi.biPlanes = 1;
    bi.biBitCount = 24;
    bi.biCompression = BI_RGB;
    bi.biSizeImage = 0;
    bi.biXPelsPerMeter = 0;
    bi.biYPelsPerMeter = 0;
    bi.biClrUsed = 0;
    bi.biClrImportant = 0;

    DWORD dwBmpSize = ((width * bi.biBitCount + 31) / 32) * 4 * height;

    // Create file header
    DWORD dwSizeofDIB =
        dwBmpSize + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bmfHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bmfHeader.bfSize = dwSizeofDIB;
    bmfHeader.bfType = 0x4D42; // BM
    bmfHeader.bfReserved1 = 0;
    bmfHeader.bfReserved2 = 0;

    // Get bitmap bits
    std::vector<char> lpbitmap(dwBmpSize);
    HDC hdc = GetDC(NULL);
    GetDIBits(hdc, hBitmap, 0, height, lpbitmap.data(),
              reinterpret_cast<BITMAPINFO *>(&bi), DIB_RGB_COLORS);
    ReleaseDC(NULL, hdc);

    // Write to file
    std::ofstream file(filename, std::ios::binary);
    if (file) {
      file.write(reinterpret_cast<char *>(&bmfHeader),
                 sizeof(BITMAPFILEHEADER));
      file.write(reinterpret_cast<char *>(&bi), sizeof(BITMAPINFOHEADER));
      file.write(lpbitmap.data(), dwBmpSize);
    }
  }
#endif
};

} // namespace Sandbox
} // namespace NeuroForge

#pragma once

#include <vector>
#include <cstdint>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#ifdef NF_HAVE_OPENCV
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#endif

namespace NeuroForge {
namespace IO {

class ScreenCapturer {
public:
    struct Rect { int x{0}; int y{0}; int w{1280}; int h{720}; };

    ScreenCapturer() = default;
    explicit ScreenCapturer(const Rect& r) : rect_(r) {}

    void setRect(const Rect& r) { rect_ = r; }

    std::vector<float> captureGrayGrid(int G) {
        #ifdef _WIN32
        HDC hScreen = GetDC(nullptr);
        if (!hScreen) return makeFallback(G);
        HDC hMem = CreateCompatibleDC(hScreen);
        if (!hMem) { ReleaseDC(nullptr, hScreen); return makeFallback(G); }
        HBITMAP hBmp = CreateCompatibleBitmap(hScreen, rect_.w, rect_.h);
        if (!hBmp) { DeleteDC(hMem); ReleaseDC(nullptr, hScreen); return makeFallback(G); }
        HGDIOBJ old = SelectObject(hMem, hBmp);
        BitBlt(hMem, 0, 0, rect_.w, rect_.h, hScreen, rect_.x, rect_.y, SRCCOPY);

        BITMAP bmp{}; GetObject(hBmp, sizeof(BITMAP), &bmp);
        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = bmp.bmWidth;
        bmi.bmiHeader.biHeight = -bmp.bmHeight; // top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        std::vector<std::uint8_t> pixels(static_cast<std::size_t>(bmp.bmWidth * bmp.bmHeight * 4), 0);
        GetDIBits(hMem, hBmp, 0, static_cast<UINT>(bmp.bmHeight), pixels.data(), &bmi, DIB_RGB_COLORS);

        SelectObject(hMem, old);
        DeleteObject(hBmp);
        DeleteDC(hMem);
        ReleaseDC(nullptr, hScreen);

        #ifdef NF_HAVE_OPENCV
        cv::Mat img(bmp.bmHeight, bmp.bmWidth, CV_8UC4, pixels.data());
        cv::Mat gray, resized;
        cv::cvtColor(img, gray, cv::COLOR_BGRA2GRAY);
        cv::resize(gray, resized, cv::Size(G, G), 0, 0, cv::INTER_AREA);
        std::vector<float> out(static_cast<std::size_t>(G * G), 0.0f);
        for (int r = 0; r < G; ++r) {
            for (int c = 0; c < G; ++c) {
                out[static_cast<std::size_t>(r * G + c)] = resized.at<unsigned char>(r, c) / 255.0f;
            }
        }
        return out;
        #else
        // Fallback simple downsample without OpenCV
        std::vector<float> out(static_cast<std::size_t>(G * G), 0.0f);
        int step_x = rect_.w / G; if (step_x <= 0) step_x = 1;
        int step_y = rect_.h / G; if (step_y <= 0) step_y = 1;
        for (int r = 0; r < G; ++r) {
            for (int c = 0; c < G; ++c) {
                std::size_t base = static_cast<std::size_t>(((r * step_y) * rect_.w + (c * step_x)) * 4);
                if (base + 2 < pixels.size()) {
                    float b = pixels[base + 0] / 255.0f;
                    float g = pixels[base + 1] / 255.0f;
                    float rr = pixels[base + 2] / 255.0f;
                    out[static_cast<std::size_t>(r * G + c)] = 0.114f * b + 0.587f * g + 0.299f * rr;
                }
            }
        }
        return out;
        #endif
        #else
        return makeFallback(G);
        #endif
    }

private:
    Rect rect_{};

    static std::vector<float> makeFallback(int G) {
        std::vector<float> grid(static_cast<std::size_t>(G * G), 0.0f);
        for (int r = 0; r < G; ++r) {
            for (int c = 0; c < G; ++c) {
                bool on = ((r / 2) % 2) ^ ((c / 2) % 2);
                grid[static_cast<std::size_t>(r * G + c)] = on ? 1.0f : 0.0f;
            }
        }
        return grid;
    }
};

} // namespace IO
} // namespace NeuroForge


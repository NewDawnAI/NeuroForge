#include "encoders/VisionEncoder.h"
#include "encoders/AudioEncoder.h"
#include <vector>
#include <cmath>
#include <iostream>
#include <string>
#include <algorithm>

using namespace NeuroForge;
using namespace NeuroForge::Encoders;

namespace {
    bool check(bool condition, const std::string& msg) {
        if (!condition) {
            std::cerr << "[FAIL] " << msg << std::endl;
            return false;
        }
        return true;
    }

    bool approxEqual(float a, float b, float eps = 1e-3f) {
        return std::fabs(a - b) <= eps;
    }

    bool testVisionEncoderEdgeResponse() {
        std::cout << "Running testVisionEncoderEdgeResponse..." << std::endl;
        VisionEncoder::Config cfg;
        cfg.grid_size = 8;     // 8x8 grid
        cfg.use_edge = true;
        cfg.edge_weight = 0.6f;
        cfg.intensity_weight = 0.4f;
        VisionEncoder enc(cfg);

        const int G = cfg.grid_size;
        const int N = G * G;
        std::vector<float> img(N, 0.0f);
        // Vertical step edge at c == G/2
        for (int r = 0; r < G; ++r) {
            for (int c = 0; c < G; ++c) {
                img[r * G + c] = (c >= G / 2) ? 1.0f : 0.0f;
            }
        }

        auto out = enc.encode(img);
        bool ok = true;
        ok &= check(static_cast<int>(out.size()) == N, "VisionEncoder output length should match input length");
        // Range check
        for (float v : out) {
            ok &= check(v >= 0.0f - 1e-6f && v <= 1.0f + 1e-6f, "VisionEncoder outputs must be within [0,1]");
        }
        // Choose a mid row to inspect values across the step
        int r = G / 2;
        int left_interior_idx = r * G + (G / 4);
        int border_left_idx   = r * G + (G / 2 - 1); // left-of-boundary pixel sees strong +dx
        int right_interior_idx= r * G + (3 * G / 4);

        float v_left = out[left_interior_idx];
        float v_border_left = out[border_left_idx];
        float v_right = out[right_interior_idx];

        // Expectations from implementation:
        // - Left interior ~ 0 (0.4*0 + 0.6*edge(~0))
        // - Right interior ~ 0.4 (0.4*1 + 0.6*0)
        // - Left-of-edge ~ 0.6 (0.4*0 + 0.6*1) after edge normalization
        ok &= check(v_left <= 0.05f, "Left interior should be near 0");
        ok &= check(v_right >= 0.3f && v_right <= 0.5f, "Right interior should reflect intensity weight (~0.4)");
        ok &= check(v_border_left > v_right + 0.1f, "Edge response should exceed right interior by a margin");
        ok &= check(v_border_left >= 0.5f, "Edge response should be significantly high (>=0.5)");

        std::cout << (ok ? "[PASS] " : "[FAIL] ") << "testVisionEncoderEdgeResponse" << std::endl;
        return ok;
    }

    std::vector<float> makeSine(int sample_rate, float freq_hz, int N, float amp=0.8f) {
        std::vector<float> x(N);
        const float dt = 1.0f / static_cast<float>(sample_rate);
        for (int n = 0; n < N; ++n) {
            x[n] = amp * std::sin(2.0f * 3.1415926535f * freq_hz * (n * dt));
        }
        return x;
    }

    bool testAudioEncoderShapesAndNormalization() {
        std::cout << "Running testAudioEncoderShapesAndNormalization..." << std::endl;
        AudioEncoder::Config cfg;
        cfg.sample_rate = 16000;
        cfg.feature_bins = 128;
        cfg.spectral_bins = 32;
        cfg.mel_bands = 32;
        cfg.pre_emphasis = true;
        AudioEncoder enc(cfg);

        bool ok = true;

        // Case 1: All zeros input -> expect all-zero features (no NaN, in-range)
        std::vector<float> z(1024, 0.0f);
        auto out0 = enc.encode(z);
        ok &= check(static_cast<int>(out0.size()) == cfg.feature_bins, "AudioEncoder zero-input: feature length must equal feature_bins");
        float sum0 = 0.0f, max0 = 0.0f, min0 = 1.0f;
        for (float v : out0) { sum0 += v; max0 = std::max(max0, v); min0 = std::min(min0, v); }
        ok &= check(sum0 == 0.0f && max0 == 0.0f && min0 == 0.0f, "AudioEncoder zero-input: all outputs should be 0");

        // Case 2: A clean sine tone -> expect non-zero features in [0,1] and max close to 1 after normalization
        auto sine = makeSine(cfg.sample_rate, 440.0f, 1024);
        auto out1 = enc.encode(sine);
        ok &= check(static_cast<int>(out1.size()) == cfg.feature_bins, "AudioEncoder sine-input: feature length must equal feature_bins");
        float sum1 = 0.0f, max1 = 0.0f, min1 = 1.0f;
        for (float v : out1) {
            ok &= check(v >= 0.0f - 1e-6f && v <= 1.0f + 1e-6f, "AudioEncoder outputs must be within [0,1]");
            sum1 += v; max1 = std::max(max1, v); min1 = std::min(min1, v);
        }
        ok &= check(sum1 > 0.0f, "AudioEncoder sine-input: features should contain energy");
        ok &= check(max1 >= 0.9f && max1 <= 1.0f + 1e-6f, "AudioEncoder sine-input: max should be normalized near 1");
        ok &= check(min1 >= 0.0f - 1e-6f, "AudioEncoder sine-input: min should be >= 0");

        std::cout << (ok ? "[PASS] " : "[FAIL] ") << "testAudioEncoderShapesAndNormalization" << std::endl;
        return ok;
    }
}

int main() {
    bool ok_all = true;
    ok_all &= testVisionEncoderEdgeResponse();
    ok_all &= testAudioEncoderShapesAndNormalization();

    if (!ok_all) {
        std::cerr << "Some encoder unit tests failed." << std::endl;
        return 1;
    }
    std::cout << "All encoder unit tests passed." << std::endl;
    return 0;
}
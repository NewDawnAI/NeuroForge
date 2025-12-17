#pragma once

#include <vector>
#include <cstddef>
#include <algorithm>
#include <cmath>
#include <numbers>

namespace NeuroForge {
namespace Encoders {

class AudioEncoder {
public:
    struct Config {
        int sample_rate = 16000;
        int feature_bins = 256;  // desired output length
        int spectral_bins = 64;  // Goertzel bins across 0..Nyquist
        int mel_bands = 64;      // number of triangular mel filters
        bool pre_emphasis = true;
    };

    explicit AudioEncoder(const Config& cfg) : cfg_(cfg) {}

    // Input: recent audio samples in range roughly [-1,1]
    // Output: feature vector of length feature_bins in [0,1]
    std::vector<float> encode(const std::vector<float>& samples) const {
        if (samples.empty()) return std::vector<float>(cfg_.feature_bins, 0.0f);

        // Copy and optionally apply pre-emphasis and Hann window
        std::vector<float> x(samples.begin(), samples.end());
        const std::size_t N = x.size();
        if (cfg_.pre_emphasis && N >= 2) {
            const float a = 0.97f;
            for (std::size_t n = N - 1; n > 0; --n) {
                x[n] = x[n] - a * x[n - 1];
            }
            x[0] = x[0];
        }
        // Hann window (guard N < 2 to avoid division by zero)
        if (N >= 2) {
            for (std::size_t n = 0; n < N; ++n) {
                float w = 0.5f * (1.0f - std::cos(2.0f * 3.14159265359f * static_cast<float>(n) / static_cast<float>(N - 1)));
                x[n] *= w;
            }
        } else {
            // For single-sample input, skip windowing
            // Ensures numerical safety without altering value
        }

        // Compute coarse spectrum via Goertzel at evenly spaced freqs up to Nyquist
        const int K = std::max(4, cfg_.spectral_bins);
        std::vector<float> mag(K, 0.0f);
        const float nyquist = static_cast<float>(std::max(1, cfg_.sample_rate)) * 0.5f;
        for (int k = 0; k < K; ++k) {
            // Avoid DC by starting slightly above 0 Hz
            float fk = (static_cast<float>(k) + 1.0f) * (nyquist / static_cast<float>(K + 1));
            float omega = 2.0f * 3.14159265359f * fk / static_cast<float>(std::max(1, cfg_.sample_rate));
            float coeff = 2.0f * std::cos(omega);
            float s_prev = 0.0f, s_prev2 = 0.0f;
            for (std::size_t n = 0; n < N; ++n) {
                float s = x[n] + coeff * s_prev - s_prev2;
                s_prev2 = s_prev;
                s_prev = s;
            }
            float power = s_prev2 * s_prev2 + s_prev * s_prev - coeff * s_prev * s_prev2;
            mag[k] = std::max(0.0f, power);
        }

        // Mel filter bank over Goertzel magnitudes
        const int M = std::max(4, cfg_.mel_bands);
        std::vector<float> mel(M, 0.0f);
        auto hz_to_mel = [](float f) { return 2595.0f * std::log10(1.0f + f / 700.0f); };
        auto mel_to_hz = [](float m) { return 700.0f * (std::pow(10.0f, m / 2595.0f) - 1.0f); };

        float mel_min = hz_to_mel(0.0f);
        float mel_max = hz_to_mel(nyquist);
        std::vector<float> mel_edges(M + 2);
        for (int m = 0; m < M + 2; ++m) {
            mel_edges[m] = mel_min + (mel_max - mel_min) * static_cast<float>(m) / static_cast<float>(M + 1);
        }
        // Map mel edges to nearest Goertzel bin indices
        std::vector<int> bin_edges(M + 2);
        for (int m = 0; m < M + 2; ++m) {
            float f = mel_to_hz(mel_edges[m]);
            int b = static_cast<int>(std::round((f / nyquist) * static_cast<float>(K + 1))) - 1;
            b = std::clamp(b, 0, K - 1);
            bin_edges[m] = b;
        }
        // Triangular filters
        for (int m = 0; m < M; ++m) {
            int b0 = bin_edges[m];
            int b1 = bin_edges[m + 1];
            int b2 = bin_edges[m + 2];
            if (!(b0 < b1 && b1 < b2)) continue;
            float inv1 = 1.0f / std::max(1, b1 - b0);
            float inv2 = 1.0f / std::max(1, b2 - b1);
            float e = 0.0f;
            for (int b = b0; b < b2; ++b) {
                float w = 0.0f;
                if (b < b1) w = (static_cast<float>(b - b0) * inv1);
                else        w = (static_cast<float>(b2 - b) * inv2);
                w = std::max(0.0f, std::min(1.0f, w));
                e += mag[b] * w;
            }
            mel[m] = e;
        }
        // Log-compress and normalize mel energies
        float max_e = 0.0f;
        for (auto& v : mel) { v = std::log1p(v); max_e = std::max(max_e, v); }
        if (max_e > 1e-9f) {
            for (auto& v : mel) v = v / max_e;
        }

        // Upsample/interpolate mel to feature_bins
        std::vector<float> out(cfg_.feature_bins, 0.0f);
        if (cfg_.feature_bins <= M) {
            // Downsample by picking evenly
            for (int i = 0; i < cfg_.feature_bins; ++i) {
                float t = (static_cast<float>(i) + 0.5f) * static_cast<float>(M) / static_cast<float>(cfg_.feature_bins);
                int idx = std::clamp(static_cast<int>(t), 0, M - 1);
                out[i] = mel[idx];
            }
        } else {
            for (int i = 0; i < cfg_.feature_bins; ++i) {
                float pos = (static_cast<float>(i) * (M - 1)) / std::max(1, cfg_.feature_bins - 1);
                int i0 = static_cast<int>(std::floor(pos));
                i0 = std::clamp(i0, 0, M - 1);
                int i1 = std::clamp(i0 + 1, 0, M - 1);
                float alpha = pos - static_cast<float>(i0);
                out[i] = (1.0f - alpha) * mel[i0] + alpha * mel[i1];
            }
        }
        // Clamp to [0,1]
        for (auto& v : out) v = std::clamp(v, 0.0f, 1.0f);
        return out;
    }

private:
    Config cfg_{};
};

} // namespace Encoders
} // namespace NeuroForge

#pragma once

#include <vector>
#include <cstddef>
#include <algorithm>
#include <cmath>

namespace NeuroForge {
namespace Encoders {

class VisionEncoder {
public:
    struct Config {
        int grid_size = 16;              // expected sqrt of input length
        bool use_edge = true;            // include simple edge magnitude
        float edge_weight = 0.6f;        // weight of edge magnitude in fusion
        float intensity_weight = 0.4f;   // weight of raw intensity in fusion
        bool use_motion = false;         // fuse temporal difference between frames
        float motion_weight = 0.3f;      // weight of motion term in fusion
        float temporal_decay = 0.9f;     // reserved for future EMA of motion
    };

    explicit VisionEncoder(const Config& cfg) : cfg_(cfg) {}

    // Input: grayscale intensities in [0,1], length must be grid_size*grid_size
    // Output: feature vector of the same length (fused intensity + edges)
    std::vector<float> encode(const std::vector<float>& gray) {
        const std::size_t N = gray.size();
        const int G = cfg_.grid_size;
        if (N == 0 || G <= 0 || static_cast<std::size_t>(G * G) != N) {
            return gray; // fallback: pass-through
        }

        std::vector<float> edges(N, 0.0f);
        if (cfg_.use_edge) {
            // Simple gradient magnitude using right and down differences
            for (int r = 0; r < G; ++r) {
                for (int c = 0; c < G; ++c) {
                    const int idx = r * G + c;
                    const float center = gray[idx];
                    float dx = 0.0f;
                    float dy = 0.0f;
                    if (c + 1 < G) {
                        dx = gray[r * G + (c + 1)] - center;
                    }
                    if (r + 1 < G) {
                        dy = gray[(r + 1) * G + c] - center;
                    }
                    float mag = std::sqrt(dx * dx + dy * dy);
                    edges[idx] = mag;
                }
            }
            // Normalize edges to [0,1]
            float max_e = 0.0f;
            for (float v : edges) max_e = std::max(max_e, std::abs(v));
            if (max_e > 1e-6f) {
                for (auto& v : edges) v = std::clamp(v / max_e, 0.0f, 1.0f);
            }
        }

        // Optional motion term: absolute frame difference
        std::vector<float> motion;
        if (cfg_.use_motion) {
            motion.resize(N, 0.0f);
            if (!last_gray_.empty() && last_gray_.size() == N) {
                for (std::size_t i = 0; i < N; ++i) {
                    motion[i] = std::abs(gray[i] - last_gray_[i]);
                }
                float max_m = 0.0f;
                for (float v : motion) max_m = std::max(max_m, v);
                if (max_m > 1e-6f) {
                    for (auto& v : motion) v = std::clamp(v / max_m, 0.0f, 1.0f);
                }
            }
        }

        std::vector<float> out(N, 0.0f);
        if (cfg_.use_edge) {
            const float ew = cfg_.edge_weight;
            const float iw = cfg_.intensity_weight;
            const float mw = cfg_.use_motion ? cfg_.motion_weight : 0.0f;
            for (std::size_t i = 0; i < N; ++i) {
                float v = iw * gray[i] + ew * edges[i] + mw * (cfg_.use_motion && i < motion.size() ? motion[i] : 0.0f);
                out[i] = std::clamp(v, 0.0f, 1.0f);
            }
        } else {
            out = gray; // pass-through intensity only
        }

        // Update last frame for motion
        last_gray_ = gray;
        return out;
    }

private:
    Config cfg_{};
    std::vector<float> last_gray_;
};

} // namespace Encoders
} // namespace NeuroForge

#include "encoders/EncoderAdapter.h"
#include <random>

namespace NeuroForge {
namespace Encoders {

static float rnd(std::uint64_t seed) {
    seed ^= seed << 13; seed ^= seed >> 7; seed ^= seed << 17;
    return (seed % 10000) / 10000.0f;
}

Embedding PlaceholderTextEncoder::encode(const std::string& text) {
    Embedding e;
    e.values.resize(64);
    std::uint64_t s = 1469598103934665603ull;
    for (char c : text) s ^= static_cast<unsigned char>(c), s *= 1099511628211ull;
    for (std::size_t i = 0; i < e.values.size(); ++i) {
        e.values[i] = rnd(s + i);
    }
    return e;
}

Embedding PlaceholderVisionEncoder::encode(const std::string& image_path) {
    Embedding e;
    e.values.resize(64);
    std::uint64_t s = 1469598103934665603ull;
    for (char c : image_path) s ^= static_cast<unsigned char>(c), s *= 1099511628211ull;
    for (std::size_t i = 0; i < e.values.size(); ++i) {
        e.values[i] = rnd(s + i * 3);
    }
    return e;
}

} // namespace Encoders
} // namespace NeuroForge


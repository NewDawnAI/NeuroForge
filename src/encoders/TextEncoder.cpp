#include "encoders/TextEncoder.h"
#include <algorithm>
#include <cmath>
#include <functional>

namespace NeuroForge {
namespace Encoders {

TextEncoder::TextEncoder(const TextEncoderConfig &config) : config_(config) {}

std::vector<float> TextEncoder::encode(const std::string &text) {
  std::vector<float> embedding(config_.output_dim, 0.0f);

  if (text.empty()) {
    return embedding;
  }

  // Deterministic "SimHash"-style projection for v1.
  // This allows the brain to recognize repeated text patterns without an LLM.
  // It is NOT semantic (yet), but it is STABLE.

  // 1. Sliding window (3-grams)
  std::hash<std::string> hasher;
  for (size_t i = 0; i < text.length(); ++i) {
    // Create simple n-grams or token-like chunks
    std::string chunk;
    // Take up to 5 chars or word boundaries
    size_t len = 0;
    while (i + len < text.length() && len < 5) {
      char c = text[i + len];
      chunk += c;
      if (std::isspace(c))
        break;
      len++;
    }

    // Hash bucket
    size_t h = hasher(chunk);

    // Project hash to vector dimensions (Random Projection equivalent)
    // We use the bits of the hash to determine sign and index contribution
    for (size_t d = 0; d < config_.output_dim; ++d) {
      // A primitive pseudo-random generator based on (hash, dim)
      size_t mix = h ^ (d * 0x9e3779b97f4a7c15ULL);
      mix = (mix ^ (mix >> 30)) * 0xbf58476d1ce4e5b9ULL;
      mix = (mix ^ (mix >> 27)) * 0x94d049bb133111ebULL;
      mix = mix ^ (mix >> 31);

      float sign = (mix & 1) ? 1.0f : -1.0f;
      float mag = static_cast<float>((mix >> 1) & 0xFF) / 255.0f;

      embedding[d] += sign * mag;
    }

    i += std::max<size_t>(1, len - 1);
  }

  // Normalize
  float norm = 0.0f;
  for (float v : embedding)
    norm += v * v;
  norm = std::sqrt(norm);

  if (norm > 1e-6f) {
    for (float &v : embedding)
      v /= norm;
  }

  return embedding;
}

} // namespace Encoders
} // namespace NeuroForge

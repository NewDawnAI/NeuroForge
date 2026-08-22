#pragma once

#include <cstdint>
#include <string>
#include <vector>


namespace NeuroForge {
namespace Encoders {

struct TextEncoderConfig {
  std::size_t output_dim = 64; // Dimension of the linguistic vector
                               // In future: Path to LLM model or tokenizer
};

class TextEncoder {
public:
  explicit TextEncoder(const TextEncoderConfig &config);
  virtual ~TextEncoder() = default;

  // Encodes a text string into a float vector of size output_dim.
  // This process determines the "Linguistic Bias" input to the World Model.
  std::vector<float> encode(const std::string &text);

private:
  TextEncoderConfig config_;
};

} // namespace Encoders
} // namespace NeuroForge

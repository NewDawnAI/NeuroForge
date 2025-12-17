#pragma once

#include <string>
#include <vector>

namespace NeuroForge {
namespace Encoders {

struct Embedding {
    std::vector<float> values;
};

class ITextEncoder {
public:
    virtual ~ITextEncoder() = default;
    virtual Embedding encode(const std::string& text) = 0;
};

class IVisionEncoder {
public:
    virtual ~IVisionEncoder() = default;
    virtual Embedding encode(const std::string& image_path) = 0;
};

class PlaceholderTextEncoder : public ITextEncoder {
public:
    Embedding encode(const std::string& text) override;
};

class PlaceholderVisionEncoder : public IVisionEncoder {
public:
    Embedding encode(const std::string& image_path) override;
};

} // namespace Encoders
} // namespace NeuroForge


# Encoders Analysis

This document analyzes the Encoders used in NeuroForge to translate raw sensory data into neural representations. Located in `src/encoders` and `include/encoders`.

## 1. VisionEncoder

### Location
- `include/encoders/VisionEncoder.h`
- `src/encoders/VisionEncoder.cpp`

### Functional Description
Transforms raw 2D image data into a 1D feature vector suitable for neural input.

### Input/Output
- **Input**: `std::vector<float>` representing grayscale pixel intensities (0.0 - 1.0).
- **Output**: `std::vector<float>` of the same size, containing fused feature data.

### Features
- **Edge Detection**: Computes gradient magnitude (Sobel-like or simple difference).
- **Motion Detection**: Computes the absolute difference between the current frame and the previous frame.
- **Fusion**: Combines Intensity, Edge, and Motion channels into a single scalar value per pixel (weighted sum).

### Usage Example
```cpp
VisionEncoder::Config cfg;
cfg.use_edge = true;
cfg.use_motion = true;
VisionEncoder encoder(cfg);
auto features = encoder.encode(raw_pixels);
```

---

## 2. AudioEncoder

### Location
- `include/encoders/AudioEncoder.h`
- `src/encoders/AudioEncoder.cpp`

### Functional Description
Transforms raw audio samples into spectral features.

### Features
- **FFT/MFCC**: Likely computes Fast Fourier Transform or Mel-Frequency Cepstral Coefficients.
- **Temporal Smoothing**: May apply a window function to smooth input over time.

### Integration
- Feeds directly into the `AuditoryCortex` region.

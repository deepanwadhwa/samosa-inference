#include "image_pixels.h"
#include <algorithm>
#include <cmath>

namespace samosa::decision {
namespace {
constexpr int side = 224;
constexpr int precision = 22;
struct Kernel { int first; std::vector<int32_t> weights; };
std::vector<Kernel> kernels(int input) {
  const double scale = static_cast<double>(input) / side;
  const double support = std::max(1.0, scale);
  std::vector<Kernel> result;
  result.reserve(side);
  for (int target = 0; target < side; ++target) {
    const double center = (target + 0.5) * scale;
    const int first = std::max(0, static_cast<int>(center - support + 0.5));
    const int last = std::min(input, static_cast<int>(center + support + 0.5));
    std::vector<double> weights;
    double total = 0;
    for (int source = first; source < last; ++source) {
      double value = std::max(0.0, 1.0 - std::abs((source - center + 0.5) / support));
      weights.push_back(value); total += value;
    }
    Kernel kernel{first, {}};
    for (auto weight : weights)
      kernel.weights.push_back(static_cast<int32_t>(weight / total * (1 << precision) + 0.5));
    result.push_back(std::move(kernel));
  }
  return result;
}
uint8_t rounded(int64_t value) {
  return static_cast<uint8_t>(std::clamp<int64_t>(value >> precision, 0, 255));
}
}
bool image_pixels(const std::vector<uint8_t>& bytes, int width, int height,
                  int channels, std::vector<float>* output, std::string* error) {
  if (!output || width < 1 || height < 1 || width > 512 || height > 512 ||
      (channels != 3 && channels != 4) ||
      bytes.size() != static_cast<size_t>(width) * height * channels) {
    if (error) *error = "invalid_decision_image_pixels";
    return false;
  }
  const auto horizontal = kernels(width), vertical = kernels(height);
  std::vector<uint8_t> intermediate(side * height * 3), resized(side * side * 3);
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < side; ++x)
      for (int channel = 0; channel < 3; ++channel) {
        const auto& kernel = horizontal[x];
        int64_t sum = 1 << (precision - 1);
        for (size_t index = 0; index < kernel.weights.size(); ++index)
          sum += static_cast<int64_t>(bytes[(y * width + kernel.first + index) * channels + channel]) * kernel.weights[index];
        intermediate[(y * side + x) * 3 + channel] = rounded(sum);
      }
  for (int y = 0; y < side; ++y)
    for (int x = 0; x < side; ++x)
      for (int channel = 0; channel < 3; ++channel) {
        const auto& kernel = vertical[y];
        int64_t sum = 1 << (precision - 1);
        for (size_t index = 0; index < kernel.weights.size(); ++index)
          sum += static_cast<int64_t>(intermediate[((kernel.first + index) * side + x) * 3 + channel]) * kernel.weights[index];
        resized[(y * side + x) * 3 + channel] = rounded(sum);
      }
  std::vector<float> next(3 * side * side);
  for (int pixel = 0; pixel < side * side; ++pixel)
    for (int channel = 0; channel < 3; ++channel) {
      // Match the processor's float64 rescale followed by float32 conversion,
      // then float32 mean/std normalization, rather than fusing arithmetic.
      float value = static_cast<float>(static_cast<double>(resized[pixel * 3 + channel]) / 255.0);
      next[channel * side * side + pixel] = (value - 0.5f) / 0.5f;
    }
  *output = std::move(next);
  return true;
}
}

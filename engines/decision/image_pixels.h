#ifndef SAMOSA_DECISION_IMAGE_PIXELS_H
#define SAMOSA_DECISION_IMAGE_PIXELS_H
#include <cstdint>
#include <string>
#include <vector>
namespace samosa::decision {
// Sources owns decoding. Input is packed RGB/RGBA, at most 512 per axis.
// RGBA follows the original engine's RGB conversion (alpha is discarded).
bool image_pixels(const std::vector<uint8_t>& bytes, int width, int height,
                  int channels, std::vector<float>* output, std::string* error);
}
#endif

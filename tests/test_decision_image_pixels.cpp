#include "decision/image_pixels.h"
#include "sha256.h"
#include <cstring>
#include <iostream>
#include <stdexcept>

#define CHECK(value) do { if (!(value)) throw std::runtime_error(#value); } while (0)
// Expected tensor hashes are from the pinned original-engine reference report
// 228d574b40c0a5634bd5b937f9e5f637deea749718bc81674f1a2be36ed3c3a5.
// This lightweight gate keeps external processor parity in model-free CI.
static void verify(int width, int height, int mode, const char* expected) {
  std::vector<uint8_t> rgb, rgba;
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x) {
      uint8_t values[3] = {static_cast<uint8_t>(mode == 0 ? 255 : 0), 0,
                           static_cast<uint8_t>(mode == 1 ? 255 : 0)};
      if (mode == 2) {
        values[0] = (x * 13 + y * 3) % 256;
        values[1] = (x * 5 + y * 17) % 256;
        values[2] = (x * 7 + y * 11) % 256;
      }
      for (auto value : values) {rgb.push_back(value); rgba.push_back(value);}
      rgba.push_back(static_cast<uint8_t>((x + y) % 256));
    }
  for (int channels : {3, 4}) {
    std::vector<float> result;
    std::string error;
    CHECK(samosa::decision::image_pixels(channels == 3 ? rgb : rgba, width, height, channels, &result, &error));
    CHECK(result.size() == 3 * 224 * 224 && sizeof(float) == 4);
    Sha256 hash; sha256_init(&hash);
    // Hash canonical float32 little-endian bytes independently of host byte order.
    for (float value : result) {
      uint32_t bits; std::memcpy(&bits, &value, sizeof(bits));
      const uint8_t bytes[] = {static_cast<uint8_t>(bits), static_cast<uint8_t>(bits >> 8),
                               static_cast<uint8_t>(bits >> 16), static_cast<uint8_t>(bits >> 24)};
      sha256_update(&hash, bytes, sizeof(bytes));
    }
    unsigned char raw[32]; char hex[65]; sha256_final(&hash, raw); sha256_hex(raw, hex);
    CHECK(std::string(hex) == expected);
    const auto retained = result;
    CHECK(!samosa::decision::image_pixels({}, 0, height, channels, &result, &error));
    CHECK(!samosa::decision::image_pixels({}, width, 513, channels, &result, &error));
    CHECK(!samosa::decision::image_pixels(rgb, width, height, 2, &result, &error));
    CHECK(!samosa::decision::image_pixels({}, width, height, channels, &result, &error));
    CHECK(!samosa::decision::image_pixels(rgb, width, height, channels, nullptr, &error));
    CHECK(result == retained);
  }
}
int main() {
  try {
    verify(224, 224, 0, "da2993ebd6ba4c5d07f43231eafe33bdf5e48a609cecebdf32f4d1fc545b3317");
    verify(224, 224, 1, "7e57208539d6e369afd96a624b4407f79be1e5530574815457556b3c35892d6c");
    verify(37, 19, 1, "7e57208539d6e369afd96a624b4407f79be1e5530574815457556b3c35892d6c");
    verify(451, 173, 2, "2add203569cc423c912b476c9c0ae129d4729210a65e302471bdbec7695b30be");
    std::cout << "PASS native image pixels: 8 pinned original processor tensors; refusal/output preservation\n";
    return 0;
  } catch (const std::exception& exception) {
    std::cerr << "FAIL native image pixels: " << exception.what() << '\n'; return 1;
  }
}

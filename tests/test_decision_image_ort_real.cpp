// Actual captured tensors through the native image primitive. Not a public-provider gate.
#include "decision/ort_image.h"
#include "json.h"
#include "sha256.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

#define CHECK(value) do { if (!(value)) throw std::runtime_error(#value); } while (0)
static std::string digest(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  CHECK(input.good());
  Sha256 hash; sha256_init(&hash);
  char buffer[65536];
  while (input) {
    input.read(buffer, sizeof(buffer));
    sha256_update(&hash, buffer, static_cast<size_t>(input.gcount()));
  }
  CHECK(input.eof());
  unsigned char raw[32]; char hex[65];
  sha256_final(&hash, raw); sha256_hex(raw, hex);
  return hex;
}
int main(int argc, char** argv) {
  try {
    CHECK(argc == 6); // MODEL REPORT MODEL_SHA REPORT_SHA ABSOLUTE_TOLERANCE
    char* end = nullptr;
    double tolerance = std::strtod(argv[5], &end);
    CHECK(end && !*end && std::isfinite(tolerance) && tolerance > 0 && tolerance <= 0.01);
    CHECK(digest(argv[1]) == argv[3] && digest(argv[2]) == argv[4]);
    std::ifstream input(argv[2], std::ios::binary | std::ios::ate);
    CHECK(input.good() && input.tellg() > 0 && input.tellg() < 2097152);
    input.seekg(0); std::ostringstream body; body << input.rdbuf();
    char* arena = nullptr;
    jval* report = json_parse(body.str().c_str(), &arena);
    CHECK(report && report->t == J_OBJ);
    struct Cleanup { jval* value; char* arena; ~Cleanup(){json_free(value);free(arena);} } cleanup{report, arena};
    auto* cases = json_get(report, "cases");
    CHECK(cases && cases->t == J_ARR && cases->len == 11);
    samosa::decision::OrtImage model;
    std::string error;
    std::vector<float> logits{123};
    CHECK(!model.infer({}, {}, 1, &logits, &error));
    if (!model.load(argv[1], &error)) throw std::runtime_error(error);
    CHECK(!model.infer({}, {}, 9, &logits, &error));
    CHECK(!model.infer({}, {}, 1, &logits, &error));
    CHECK(logits == std::vector<float>{123});
    double maximum = 0;
    for (int index = 0; index < cases->len; ++index) {
      auto* item = cases->kids[index];
      auto* name = json_get(item, "name");
      auto* batches = json_get(item, "batches");
      CHECK(name && name->t == J_STR && batches && batches->t == J_ARR && batches->len == 1);
      auto* batch = batches->kids[0];
      auto* tensors = json_get(batch, "inputs");
      auto* tokens = json_get(tensors, "input_ids");
      CHECK(tokens && tokens->t == J_ARR && tokens->len > 0 && tokens->len <= 8);
      std::vector<int64_t> ids;
      for (int row = 0; row < tokens->len; ++row) {
        auto* values = tokens->kids[row];
        CHECK(values->t == J_ARR && values->len == 64);
        for (int column = 0; column < 64; ++column) {
          auto* value = values->kids[column];
          CHECK(value->t == J_NUM && std::isfinite(value->num) && value->num >= 0 &&
                value->num < 256000 && std::floor(value->num) == value->num);
          ids.push_back(static_cast<int64_t>(value->num));
        }
      }
      auto* pixel = json_get(tensors, "pixel_values");
      auto* file = json_get(pixel, "file"); auto* pin = json_get(pixel, "sha256");
      CHECK(file && file->t == J_STR && pin && pin->t == J_STR);
      std::filesystem::path relative(file->str);
      CHECK(relative.filename() == relative && !relative.is_absolute());
      auto path = std::filesystem::path(argv[2]).parent_path() / relative;
      CHECK(digest(path) == pin->str);
      std::vector<float> pixels(3 * 224 * 224);
      std::ifstream blob(path, std::ios::binary);
      blob.read(reinterpret_cast<char*>(pixels.data()), pixels.size() * sizeof(float));
      CHECK(blob.good() && blob.peek() == std::char_traits<char>::eof());
      const auto original_ids = ids;
      const auto original_pixels = pixels;
      if (!model.infer(ids, pixels, tokens->len, &logits, &error)) throw std::runtime_error(error);
      CHECK(ids == original_ids && pixels == original_pixels);
      auto* expected = json_get(batch, "logits_per_image");
      CHECK(expected && expected->t == J_ARR && expected->len == 1);
      auto* values = expected->kids[0];
      CHECK(values->t == J_ARR && values->len == tokens->len && logits.size() == static_cast<size_t>(values->len));
      double delta = 0;
      for (int column = 0; column < values->len; ++column) {
        auto* value = values->kids[column];
        CHECK(value->t == J_NUM && std::isfinite(value->num));
        delta = std::max(delta, std::abs(logits[column] - value->num));
      }
      maximum = std::max(maximum, delta);
      std::cout << name->str << " max_abs_error=" << delta << std::endl;
      CHECK(delta <= tolerance);
      auto retained = logits;
      ids[0] = 256000;
      CHECK(!model.infer(ids, pixels, tokens->len, &logits, &error) && logits == retained);
      ids = original_ids; pixels[0] = NAN;
      CHECK(!model.infer(ids, pixels, tokens->len, &logits, &error) && logits == retained);
      CHECK(!model.load("/missing/samosa-image-model.onnx", &error));
      CHECK(model.infer(original_ids, original_pixels, tokens->len, &logits, &error) && logits == retained);
    }
    std::cout << "PASS native actual image tensors: 11 cases; max_abs_error=" << maximum << '\n';
    return 0;
  } catch (const std::exception& exception) {
    std::cerr << "FAIL native image: " << exception.what() << '\n'; return 1;
  }
}

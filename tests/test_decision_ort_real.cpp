// Actual-model gate for the private native tensor primitive, not provider proof.
#include "decision/ort_nli.h"
#include "json.h"
#include "sha256.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

static unsigned checks;
#define CHECK(value) do { ++checks; if (!(value)) throw std::runtime_error(#value); } while (0)
static std::string file_hash(const char* path) {
  std::ifstream input(path, std::ios::binary);
  CHECK(input.good());
  Sha256 hash; sha256_init(&hash);
  char buffer[65536];
  while (input) {
    input.read(buffer, sizeof(buffer));
    sha256_update(&hash, buffer, static_cast<size_t>(input.gcount()));
  }
  CHECK(input.eof());
  unsigned char raw[32];char hex[65];sha256_final(&hash, raw);sha256_hex(raw, hex);
  return hex;
}
static std::vector<int64_t> matrix(jval* value, size_t* rows, size_t* columns) {
  CHECK(value && value->t == J_ARR && value->len > 0 && value->len <= 8);
  *rows = static_cast<size_t>(value->len);*columns = 0;
  std::vector<int64_t> result;
  for (int row = 0; row < value->len; ++row) {
    jval* values = value->kids[row];
    CHECK(values && values->t == J_ARR && values->len > 0 && values->len <= 512);
    if (!row) *columns = static_cast<size_t>(values->len);
    CHECK(*columns == static_cast<size_t>(values->len));
    for (int column = 0; column < values->len; ++column) {
      jval* item = values->kids[column];
      CHECK(item->t == J_NUM && std::isfinite(item->num) && item->num >= 0 &&
            item->num < 128100 && std::floor(item->num) == item->num);
      result.push_back(static_cast<int64_t>(item->num));
    }
  }
  return result;
}
int main(int argc, char** argv) {
  try {
    CHECK(argc == 6); // MODEL REPORT MODEL_SHA REPORT_SHA ABSOLUTE_TOLERANCE
    char* end = nullptr;double tolerance = std::strtod(argv[5], &end);
    CHECK(end && !*end && std::isfinite(tolerance) && tolerance > 0 && tolerance <= 0.1);
    CHECK(file_hash(argv[1]) == argv[3]);CHECK(file_hash(argv[2]) == argv[4]);
    std::ifstream input(argv[2], std::ios::binary | std::ios::ate);
    CHECK(input.good() && input.tellg() > 0 && input.tellg() <= 2097152);
    input.seekg(0);std::ostringstream body;body << input.rdbuf();
    char* arena = nullptr;jval* report = json_parse(body.str().c_str(), &arena);
    CHECK(report && report->t == J_OBJ);
    struct Cleanup {jval* value;char* arena;~Cleanup(){json_free(value);free(arena);}} cleanup{report, arena};
    samosa::decision::OrtNli model;
    std::string error;std::vector<float> logits{123};
    CHECK(!model.infer({1}, {1}, 1, 1, &logits, &error) && logits == std::vector<float>{123});
    std::cerr << "ONNX Runtime " << model.runtime_version() << '\n';
    if (!model.load(argv[1], &error)) throw std::runtime_error(error);
    CHECK(!model.infer({1}, {1}, 9, 1, &logits, &error));
    CHECK(!model.infer({1}, {1}, 1, 513, &logits, &error));
    CHECK(!model.infer({128100}, {1}, 1, 1, &logits, &error));
    CHECK(!model.infer({1}, {2}, 1, 1, &logits, &error));
    CHECK(!model.infer({1}, {0}, 1, 1, &logits, &error));
    CHECK(!model.infer({1}, {1}, 1, 1, nullptr, &error));
    CHECK(logits == std::vector<float>{123});
    jval* cases = json_get(report, "cases");CHECK(cases && cases->t == J_ARR);
    unsigned batches = 0;double maximum = 0;
    for (int index = 0; index < cases->len; ++index) {
      jval* item = cases->kids[index];jval* name = json_get(item, "name");
      jval* values = json_get(item, "batches");CHECK(name && name->t == J_STR && values && values->t == J_ARR);
      for (int batch = 0; batch < values->len; ++batch) {
        jval* recorded = values->kids[batch];jval* tensors = json_get(recorded, "inputs");
        size_t rows, columns, mask_rows, mask_columns;
        auto ids = matrix(json_get(tensors, "input_ids"), &rows, &columns);
        auto mask = matrix(json_get(tensors, "attention_mask"), &mask_rows, &mask_columns);
        CHECK(rows == mask_rows && columns == mask_columns);
        if (!model.infer(ids, mask, rows, columns, &logits, &error)) throw std::runtime_error(error);
        jval* expected = json_get(recorded, "logits");
        CHECK(expected && expected->t == J_ARR && expected->len == static_cast<int>(rows));
        CHECK(logits.size() == rows * 2);
        double delta = 0;
        for (size_t row = 0; row < rows; ++row) {
          jval* pair = expected->kids[row];CHECK(pair->t == J_ARR && pair->len == 2);
          for (size_t column = 0; column < 2; ++column) {
            jval* value = pair->kids[column];CHECK(value->t == J_NUM && std::isfinite(value->num));
            delta = std::max(delta, std::abs(logits[row * 2 + column] - value->num));
          }
        }
        maximum = std::max(maximum, delta);++batches;
        std::cerr << name->str << " batch=" << batch << " max_abs_error=" << delta << '\n';
      }
    }
    CHECK(batches == 14);CHECK(maximum <= tolerance);
    CHECK(!model.load("/missing/samosa-nli-model.onnx", &error));
    // A failed replacement leaves the original session usable.
    jval* first = json_get(cases->kids[0], "batches")->kids[0];
    size_t rows, columns, other_rows, other_columns;
    auto ids = matrix(json_get(json_get(first, "inputs"), "input_ids"), &rows, &columns);
    auto mask = matrix(json_get(json_get(first, "inputs"), "attention_mask"), &other_rows, &other_columns);
    CHECK(model.infer(ids, mask, rows, columns, &logits, &error));
    std::cout << "PASS native actual NLI tensors: " << checks << " checks; " << batches
              << " reference batches, max_abs_error=" << maximum << '\n';
    return 0;
  } catch (const std::exception& exception) {
    std::cerr << "FAIL native NLI: " << exception.what() << '\n';return 1;
  }
}

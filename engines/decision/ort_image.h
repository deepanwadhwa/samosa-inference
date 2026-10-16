#ifndef SAMOSA_DECISION_ORT_IMAGE_H
#define SAMOSA_DECISION_ORT_IMAGE_H
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace samosa::decision {
// Fixed SigLIP model signature; the owning engine verifies assets and serializes calls.
class OrtImage {
 public:
  OrtImage();
  ~OrtImage();
  OrtImage(const OrtImage&) = delete;
  OrtImage& operator=(const OrtImage&) = delete;
  bool load(const std::string& model, std::string* error);
  bool infer(const std::vector<int64_t>& ids, const std::vector<float>& pixels,
             size_t rows, std::vector<float>* logits, std::string* error);
 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
}
#endif

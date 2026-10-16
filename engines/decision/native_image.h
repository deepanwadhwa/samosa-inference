#ifndef SAMOSA_DECISION_NATIVE_IMAGE_H
#define SAMOSA_DECISION_NATIVE_IMAGE_H
#include "ort_image.h"
#include "request.h"
namespace samosa::decision {
class NativeImage {
 public:
  NativeImage() = default;
  ~NativeImage();
  NativeImage(const NativeImage&) = delete;
  NativeImage& operator=(const NativeImage&) = delete;
  bool load(const std::string& model, const std::string& tokenizer, std::string* error);
  bool infer(const std::vector<std::string>& descriptions, const std::vector<float>& pixels,
             std::vector<float>* logits, std::string* error);
 private:
  OrtImage model_;
  void* tokenizer_ = nullptr;
};
// Binds one prepared frame to the shared typed request executor. State text in
// the request is only a frame identity; it never conditions image matching.
DecisionBackend image_backend(NativeImage&, const std::vector<float>& pixels);
}
#endif

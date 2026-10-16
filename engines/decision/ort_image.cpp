#include "ort_image.h"
#include "ort_runtime.h"
#include <array>
#include <cmath>
#include <cstring>

namespace samosa::decision {
using runtime::failure;
struct OrtImage::Impl {
  Ort::Env env;
  Ort::SessionOptions options;
  Ort::Session session{nullptr};
  explicit Impl(const std::string& path) : env(runtime::environment()) {
    runtime::configure(options);
    session = Ort::Session(env, path.c_str(), options);
    if (session.GetInputCount() != 2 || session.GetOutputCount() != 1)
      throw std::runtime_error("invalid_image_model_signature");
    Ort::AllocatorWithDefaultOptions allocator;
    for (size_t index = 0; index < 2; ++index) {
      auto name = session.GetInputNameAllocated(index, allocator);
      auto type = session.GetInputTypeInfo(index);
      auto tensor = type.GetTensorTypeAndShapeInfo();
      auto shape = index ? std::vector<int64_t>{1, 3, 224, 224} : std::vector<int64_t>{-1, 64};
      auto dtype = index ? ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT : ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64;
      if (std::strcmp(name.get(), index ? "pixel_values" : "input_ids") ||
          tensor.GetElementType() != dtype || tensor.GetShape() != shape)
        throw std::runtime_error("invalid_image_input_signature");
    }
    auto name = session.GetOutputNameAllocated(0, allocator);
    auto type = session.GetOutputTypeInfo(0);
    auto tensor = type.GetTensorTypeAndShapeInfo();
    if (std::strcmp(name.get(), "logits_per_image") ||
        tensor.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT ||
        tensor.GetShape() != std::vector<int64_t>{1, -1})
      throw std::runtime_error("invalid_image_output_signature");
  }
};
OrtImage::OrtImage() = default;
OrtImage::~OrtImage() = default;
bool OrtImage::load(const std::string& model, std::string* error) {
  try {
    auto candidate = std::make_unique<Impl>(model);
    impl_ = std::move(candidate);
    return true;
  } catch (const std::exception& exception) { return failure(error, exception.what()); }
}
bool OrtImage::infer(const std::vector<int64_t>& ids, const std::vector<float>& pixels,
                     size_t rows, std::vector<float>* logits, std::string* error) {
  if (!impl_) return failure(error, "image_model_not_loaded");
  if (!logits || !rows || rows > 8 || ids.size() != rows * 64 || pixels.size() != 3 * 224 * 224)
    return failure(error, "invalid_image_tensor_shape");
  for (auto id : ids)
    if (id < 0 || id >= 256000) return failure(error, "invalid_image_token");
  for (auto pixel : pixels)
    if (!std::isfinite(pixel) || pixel < -1 || pixel > 1)
      return failure(error, "invalid_image_pixel");
  try {
    const int64_t text_shape[] = {static_cast<int64_t>(rows), 64}, image_shape[] = {1, 3, 224, 224};
    Ort::AllocatorWithDefaultOptions allocator;
    std::array<Ort::Value, 2> inputs = {
        Ort::Value::CreateTensor<int64_t>(allocator, text_shape, 2),
        Ort::Value::CreateTensor<float>(allocator, image_shape, 4)};
    std::memcpy(inputs[0].GetTensorMutableData<int64_t>(), ids.data(), ids.size() * sizeof(int64_t));
    std::memcpy(inputs[1].GetTensorMutableData<float>(), pixels.data(), pixels.size() * sizeof(float));
    const char* names[] = {"input_ids", "pixel_values"};
    const char* outputs[] = {"logits_per_image"};
    auto result = impl_->session.Run(Ort::RunOptions{nullptr}, names, inputs.data(), 2, outputs, 1);
    if (result.size() != 1 || !result[0].IsTensor())
      return failure(error, "invalid_image_runtime_output");
    auto info = result[0].GetTensorTypeAndShapeInfo();
    if (info.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT ||
        info.GetShape() != std::vector<int64_t>{1, static_cast<int64_t>(rows)})
      return failure(error, "invalid_image_runtime_output");
    const auto* raw = result[0].GetTensorData<float>();
    std::vector<float> next(raw, raw + rows);
    for (auto value : next)
      if (!std::isfinite(value)) return failure(error, "nonfinite_image_logits");
    *logits = std::move(next);
    return true;
  } catch (const std::exception& exception) { return failure(error, exception.what()); }
}
}

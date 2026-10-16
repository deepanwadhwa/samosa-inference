#include "ort_nli.h"
#include "ort_runtime.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace samosa::decision {
using runtime::failure;

struct OrtNli::Impl {
  Ort::Env env;
  Ort::SessionOptions options;
  Ort::Session session{nullptr};
  explicit Impl(const std::string& path) : env(runtime::environment()) {
    runtime::configure(options);
    // CPU is the default provider; do not opt into CoreML/CUDA or write an
    // optimized graph beside the immutable input model.
    session = Ort::Session(env, path.c_str(), options);
    if (session.GetInputCount() != 2 || session.GetOutputCount() != 1)
      throw std::runtime_error("invalid_nli_model_signature");
    Ort::AllocatorWithDefaultOptions allocator;
    for (size_t index = 0; index < 2; ++index) {
      auto name = session.GetInputNameAllocated(index, allocator);
      const char* expected = index ? "attention_mask" : "input_ids";
      auto type = session.GetInputTypeInfo(index);
      auto tensor = type.GetTensorTypeAndShapeInfo();
      if (std::strcmp(name.get(), expected) ||
          tensor.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64 ||
          tensor.GetShape() != std::vector<int64_t>({-1, -1}))
        throw std::runtime_error("invalid_nli_input_signature");
    }
    auto name = session.GetOutputNameAllocated(0, allocator);
    auto type = session.GetOutputTypeInfo(0);
    auto tensor = type.GetTensorTypeAndShapeInfo();
    if (std::strcmp(name.get(), "logits") ||
        tensor.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16 ||
        tensor.GetShape() != std::vector<int64_t>({-1, 2}))
      throw std::runtime_error("invalid_nli_output_signature");
  }
};

OrtNli::OrtNli() = default;
OrtNli::~OrtNli() = default;
std::string OrtNli::runtime_version() {
  const auto* base = OrtGetApiBase();
  return base ? base->GetVersionString() : "unavailable";
}
bool OrtNli::load(const std::string& path, std::string* error) {
  try {
    // Failed replacement must leave the previously loaded model intact.
    auto candidate = std::make_unique<Impl>(path);
    impl_ = std::move(candidate);
    return true;
  } catch (const std::exception& exception) {
    return failure(error, exception.what());
  }
}
bool OrtNli::infer(const std::vector<int64_t>& ids, const std::vector<int64_t>& mask,
                   size_t rows, size_t columns, std::vector<float>* logits,
                   std::string* error) {
  if (!impl_) return failure(error, "nli_model_not_loaded");
  if (!logits || !rows || rows > 8 || !columns || columns > 512 ||
      ids.size() != rows * columns || mask.size() != ids.size())
    return failure(error, "invalid_nli_tensor_shape");
  for (size_t index = 0; index < ids.size(); ++index)
    if (ids[index] < 0 || ids[index] >= 128100 || (mask[index] != 0 && mask[index] != 1))
      return failure(error, "invalid_nli_tensor_value");
  for (size_t row = 0; row < rows; ++row)
    if (std::none_of(mask.begin() + row * columns, mask.begin() + (row + 1) * columns,
                     [](int64_t value) { return value != 0; }))
      return failure(error, "empty_nli_attention_mask");
  try {
    int64_t shape[] = {static_cast<int64_t>(rows), static_cast<int64_t>(columns)};
    Ort::AllocatorWithDefaultOptions allocator;
    std::array<Ort::Value, 2> inputs = {
        Ort::Value::CreateTensor<int64_t>(allocator, shape, 2),
        Ort::Value::CreateTensor<int64_t>(allocator, shape, 2)};
    // Runtime-owned inputs preserve the caller's tensors on every outcome.
    std::memcpy(inputs[0].GetTensorMutableData<int64_t>(), ids.data(), ids.size() * sizeof(int64_t));
    std::memcpy(inputs[1].GetTensorMutableData<int64_t>(), mask.data(), mask.size() * sizeof(int64_t));
    const char* names[] = {"input_ids", "attention_mask"};
    const char* outputs[] = {"logits"};
    auto result = impl_->session.Run(Ort::RunOptions{nullptr}, names, inputs.data(), 2, outputs, 1);
    if (result.size() != 1 || !result[0].IsTensor())
      return failure(error, "invalid_nli_runtime_output");
    auto info = result[0].GetTensorTypeAndShapeInfo();
    if (info.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16 ||
        info.GetShape() != std::vector<int64_t>({static_cast<int64_t>(rows), 2}))
      return failure(error, "invalid_nli_runtime_output");
    const auto* raw = result[0].GetTensorData<Ort::Float16_t>();
    std::vector<float> next(rows * 2);
    for (size_t index = 0; index < next.size(); ++index) {
      next[index] = raw[index].ToFloat();
      if (!std::isfinite(next[index])) return failure(error, "nonfinite_nli_logits");
    }
    *logits = std::move(next);
    return true;
  } catch (const std::exception& exception) {
    return failure(error, exception.what());
  }
}
}  // namespace samosa::decision

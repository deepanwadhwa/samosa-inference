#ifndef SAMOSA_DECISION_ORT_RUNTIME_H
#define SAMOSA_DECISION_ORT_RUNTIME_H
#define ORT_API_MANUAL_INIT
#include <onnxruntime_cxx_api.h>
#include <mutex>
#include <stdexcept>
#include <string>

namespace samosa::decision::runtime {
inline Ort::Env environment() {
  static std::once_flag once;
  std::call_once(once, [] {
    const auto* base = OrtGetApiBase();
    const auto* api = base ? base->GetApi(ORT_API_VERSION) : nullptr;
    if (!api) throw std::runtime_error("unsupported_onnx_runtime_api");
    Ort::InitApi(api);
  });
  return Ort::Env(ORT_LOGGING_LEVEL_WARNING, "samosa-decision");
}
inline void configure(Ort::SessionOptions& options) {
  options.SetIntraOpNumThreads(4);
  options.SetInterOpNumThreads(1);
  options.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
  options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_BASIC);
}
inline bool failure(std::string* error, const char* message) {
  if (error) *error = std::string(message).substr(0, 4096);
  return false;
}
}  // namespace samosa::decision::runtime
#endif

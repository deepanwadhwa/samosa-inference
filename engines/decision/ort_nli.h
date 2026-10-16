#ifndef SAMOSA_DECISION_ORT_NLI_H
#define SAMOSA_DECISION_ORT_NLI_H
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace samosa::decision {
// Private model primitive. The provider owns file identity/admission; this
// primitive checks the fixed DeBERTa tensor signature and bounded inputs.
// Calls on one instance must be serialized by its owning engine.
class OrtNli {
 public:
  OrtNli();
  ~OrtNli();
  OrtNli(const OrtNli&) = delete;
  OrtNli& operator=(const OrtNli&) = delete;
  bool load(const std::string& model, std::string* error);
  bool infer(const std::vector<int64_t>& ids, const std::vector<int64_t>& mask,
             size_t rows, size_t columns, std::vector<float>* logits,
             std::string* error);
  static std::string runtime_version();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
}  // namespace samosa::decision
#endif

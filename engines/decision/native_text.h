#ifndef SAMOSA_DECISION_NATIVE_TEXT_H
#define SAMOSA_DECISION_NATIVE_TEXT_H
#include "ort_nli.h"
#include "text_evidence.h"
namespace samosa::decision {
struct EncodedPairs {
  size_t rows = 0, columns = 0;
  std::vector<int64_t> ids, mask, types;
};
// Private single-engine bridge. The owning engine serializes calls and verifies
// asset identities before load; no downloads or Python execution occur here.
class NativeText {
 public:
  NativeText() = default;
  ~NativeText();
  NativeText(const NativeText&) = delete;
  NativeText& operator=(const NativeText&) = delete;
  bool load(const std::string& model, const std::string& tokenizer, std::string* error);
  bool infer(const std::vector<TextPair>&, PairTruncation,
             std::vector<std::array<float, 2>>*, std::string*, EncodedPairs* observed = nullptr);
 private:
  OrtNli model_;
  void* tokenizer_ = nullptr;
};
}
#endif

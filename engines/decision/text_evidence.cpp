#include "text_evidence.h"
#include <algorithm>
#include <cmath>

namespace samosa::decision {
namespace {
bool fail(std::string* error, const char* message) {
  if (error) *error = message;
  return false;
}
// Match the reference's float32 softmax after converting model logits from
// float16. Subtracting the maximum prevents overflow without changing logits.
double probability(float entailment, float opposite) {
  const float maximum = std::max(entailment, opposite);
  const float a = std::exp(entailment - maximum);
  const float b = std::exp(opposite - maximum);
  return static_cast<double>(a / (a + b));
}
bool run(const PairInference& infer, const std::vector<TextPair>& pairs,
         PairTruncation truncation, std::vector<std::array<float, 2>>* output,
         std::string* error) {
  if (!infer || pairs.empty() || pairs.size() > 128)
    return fail(error, "invalid_evidence_input");
  std::vector<std::array<float, 2>> all;
  for (size_t start = 0; start < pairs.size(); start += 8) {
    const size_t end = std::min(start + 8, pairs.size());
    const std::vector<TextPair> batch(pairs.begin() + start, pairs.begin() + end);
    std::vector<std::array<float, 2>> logits;
    if (!infer(batch, truncation, &logits, error)) return false;
    if (logits.size() != batch.size()) return fail(error, "invalid_evidence_logits");
    for (const auto& row : logits)
      if (!std::isfinite(row[0]) || !std::isfinite(row[1]))
        return fail(error, "invalid_evidence_logits");
    all.insert(all.end(), logits.begin(), logits.end());
  }
  *output = std::move(all);
  return true;
}
}  // namespace

bool predicate(const PairInference& infer, const std::string& state,
               const std::string& instructions, double* output, std::string* error) {
  if (!output) return fail(error, "invalid_evidence_output");
  std::vector<std::array<float, 2>> logits;
  if (!run(infer, {{state, instructions}}, PairTruncation::LongestFirst, &logits, error))
    return false;
  *output = probability(logits[0][0], logits[0][1]);
  return true;
}

bool binary_predicate(const PairInference& infer, const std::string& state,
                      const std::string& true_statement,
                      const std::string& false_statement, double* output,
                      std::string* error) {
  if (!output) return fail(error, "invalid_evidence_output");
  // The original engine executes these separately, without batch padding.
  std::vector<std::array<float, 2>> yes, no;
  if (!run(infer, {{state, true_statement}}, PairTruncation::LongestFirst, &yes, error) ||
      !run(infer, {{state, false_statement}}, PairTruncation::LongestFirst, &no, error))
    return false;
  *output = probability(yes[0][0], no[0][0]);
  return true;
}

bool relations(const PairInference& infer, const std::vector<RelationInput>& items,
               double threshold, std::vector<RelationResult>* output,
               std::string* error) {
  if (!output) return fail(error, "invalid_evidence_output");
  if (!std::isfinite(threshold) || threshold <= 0 || threshold >= 1 || items.size() > 64)
    return fail(error, "invalid_evidence_input");
  if (items.empty()) { output->clear(); return true; }
  std::vector<TextPair> pairs;
  for (const auto& item : items) {
    pairs.push_back({item.state, item.proposition});
    pairs.push_back({item.state, item.contradiction});
  }
  std::vector<std::array<float, 2>> logits;
  if (!run(infer, pairs, PairTruncation::OnlyFirst, &logits, error)) return false;
  std::vector<RelationResult> results;
  for (size_t i = 0; i < items.size(); ++i) {
    RelationResult result;
    result.supports = probability(logits[2*i][0], logits[2*i][1]);
    result.contradicts = probability(logits[2*i+1][0], logits[2*i+1][1]);
    result.threshold = threshold;
    const bool support = result.supports >= threshold;
    const bool contradiction = result.contradicts >= threshold;
    result.relation = support ? (contradiction ? "conflicted" : "supports") :
                               (contradiction ? "contradicts" : "unknown");
    results.push_back(std::move(result));
  }
  *output = std::move(results);
  return true;
}
}  // namespace samosa::decision

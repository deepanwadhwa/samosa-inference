#ifndef SAMOSA_DECISION_TEXT_EVIDENCE_H
#define SAMOSA_DECISION_TEXT_EVIDENCE_H
#include <array>
#include <functional>
#include <string>
#include <vector>

namespace samosa::decision {
struct TextPair { std::string premise, hypothesis; };
enum class PairTruncation { LongestFirst, OnlyFirst };
// The callback must preserve order and return two finite logits per pair:
// entailment first, not-entailment second. Each call contains at most 8 pairs.
using PairInference = std::function<bool(const std::vector<TextPair>&,
    PairTruncation, std::vector<std::array<float, 2>>*, std::string*)>;
struct RelationInput { std::string state, proposition, contradiction; };
struct RelationResult {
  std::string relation;
  double supports = 0, contradicts = 0, threshold = 0.5;
};
struct Candidate { std::string name, description; };
struct ChoiceResult {
  std::string choice;
  std::vector<double> probabilities; // Same order as the caller's candidates.
  double confidence = 0;
};
struct ScoreResult {
  double score = 0, confidence = 0;
  std::vector<double> probabilities; // Zero-based ordered scale positions.
};

// Private typed scoring for the existing two-label DeBERTa checkpoint. States
// are already serialized by the caller; structured-state rendering belongs to
// the typed request layer. Results are published only after complete success.
bool predicate(const PairInference&, const std::string& state,
               const std::string& instructions, double*, std::string*);
// Explicit true/false criteria ignore instructions, as in OpenDecision.noul.
bool binary_predicate(const PairInference&, const std::string& state,
                      const std::string& true_statement,
                      const std::string& false_statement, double*, std::string*);
bool relations(const PairInference&, const std::vector<RelationInput>&,
               double threshold, std::vector<RelationResult>*, std::string*);
bool choice(const PairInference&, const std::string& state,
            const std::string& instructions, const std::vector<Candidate>&,
            ChoiceResult*, std::string*);
bool score(const PairInference&, const std::string& state,
           const std::string& instructions, const std::vector<std::string>& levels,
           ScoreResult*, std::string*);
}  // namespace samosa::decision
#endif

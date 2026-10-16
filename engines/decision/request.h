#ifndef SAMOSA_DECISION_TYPED_REQUEST_H
#define SAMOSA_DECISION_TYPED_REQUEST_H
#include "text_evidence.h"
namespace samosa::decision {
// Typed request parsing/serialization is shared across model modalities.
struct DecisionBackend {
  std::function<bool(const std::string&, const std::string&, const std::vector<Candidate>&, ChoiceResult*, std::string*)> choose;
  std::function<bool(const std::string&, const std::string&, const std::vector<std::string>&, ScoreResult*, std::string*)> scale;
  std::function<bool(const std::string&, const std::string&, double*, std::string*)> predicate;
  std::function<bool(const std::string&, const std::string&, const std::string&, double*, std::string*)> binary;
  std::function<bool(const std::vector<RelationInput>&, double, std::vector<RelationResult>*, std::string*)> relate;
  std::string relation_backend;
};
bool execute_request(const DecisionBackend&, const std::string& json,
                     std::string* output, std::string* error);
// Strict bounded JSON -> existing typed result JSON. The callback owns model
// execution/control checks. Failures preserve the caller's previous output.
bool execute_request(const PairInference&, const std::string& json,
                     std::string* output, std::string* error);
}
#endif

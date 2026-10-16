#include "text_evidence.h"
#include "evidence_math.h"
#include <algorithm>
#include <cmath>
#include <unordered_set>

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

size_t winner(const std::vector<double>& probabilities) {
  // Python max(dict, key=dict.get) selects the first candidate on equality.
  return static_cast<size_t>(std::max_element(probabilities.begin(), probabilities.end()) - probabilities.begin());
}
bool distribution(const PairInference& infer, const std::vector<TextPair>& pairs,
                  std::vector<double>* output, std::string* error) {
  std::vector<std::array<float, 2>> logits;
  if (!run(infer, pairs, PairTruncation::OnlyFirst, &logits, error)) return false;
  // The HF zero-shot pipeline uses float32 exp and a candidate-wise sum.
  // These fixed float16 model logits are bounded well below float32 overflow.
  std::vector<float> weights; float sum = 0;
  for (const auto& row : logits) {
    const float weight = std::exp(row[0]);
    if (!std::isfinite(weight)) return fail(error, "invalid_evidence_distribution");
    weights.push_back(weight); sum += weight;
  }
  if (!std::isfinite(sum) || sum <= 0) return fail(error, "invalid_evidence_distribution");
  std::vector<double> result;
  for (float weight : weights) result.push_back(static_cast<double>(weight / sum));
  *output = std::move(result); return true;
}
// The reference interpolates instructions into a Python format template and
// supplies exactly one positional argument, consumed by the final {} field.
// Additional unescaped fields therefore fail (missing argument, mixed numbering
// or missing keyword). Doubled braces in instructions are literal braces.
bool question(const std::string& instructions, const std::string& label,
              std::string* output, std::string* error) {
  std::string literal;
  for (size_t i=0;i<instructions.size();++i) {
    const char c=instructions[i];
    if (c=='{' || c=='}') {
      if (i+1>=instructions.size() || instructions[i+1]!=c)
        return fail(error,"invalid_question_template");
      ++i;
    }
    literal+=c;
  }
  *output="The answer to the question \""+literal+"\" is "+label+".";
  return true;
}
bool profile(const PairInference& infer, const std::string& state,
             const std::string& instructions, const std::vector<Candidate>& candidates,
             int mode, std::vector<double>* probabilities, std::string* error) {
  std::unordered_set<std::string> rendered;
  std::vector<TextPair> pairs;
  for (const auto& candidate : candidates) {
    std::string natural=candidate.name;
    std::replace(natural.begin(),natural.end(),'_',' ');
    std::string label=mode==2 ? natural : candidate.description.empty() ? natural :
        mode==0 ? candidate.description : natural+": "+candidate.description;
    if (rendered.count(label)) label=natural+": "+label;
    if (!rendered.insert(label).second) return fail(error,"ambiguous_candidate_labels");
    std::string hypothesis;
    if (mode==0) { if (!question(instructions,label,&hypothesis,error)) return false; }
    else hypothesis="This example is "+label+".";
    // HF validates formatting with the first label before building any pairs.
    const std::string template_text=mode==0 ?
        "The answer to the question \""+instructions+"\" is {}." : "This example is {}.";
    if (pairs.empty() && hypothesis==template_text) return fail(error,"invalid_question_template");
    pairs.push_back({mode==2 ? state+"\n\nQuestion: "+instructions : state,hypothesis});
  }
  return distribution(infer,pairs,probabilities,error);
}
}  // namespace

bool choice(const PairInference& infer, const std::string& state,
            const std::string& instructions, const std::vector<Candidate>& candidates,
            ChoiceResult* output, std::string* error) {
  if (!output) return fail(error,"invalid_evidence_output");
  if (candidates.size()<2 || candidates.size()>128) return fail(error,"invalid_evidence_input");
  std::unordered_set<std::string> names;
  for (const auto& candidate : candidates)
    if (!names.insert(candidate.name).second) return fail(error,"duplicate_candidate_name");
  std::vector<double> a,b;
  if (!profile(infer,state,instructions,candidates,0,&a,error) ||
      !profile(infer,state,instructions,candidates,1,&b,error)) return false;
  size_t selected=winner(a);const size_t alternate=winner(b);
  ChoiceResult result;
  if (selected==alternate) {
    for (size_t i=0;i<a.size();++i) result.probabilities.push_back((a[i]+b[i])/2);
  } else {
    std::vector<Candidate> disputed;std::vector<size_t> indices;
    for (size_t i=0;i<candidates.size();++i) if (i==selected || i==alternate) {
      disputed.push_back(candidates[i]);indices.push_back(i);
    }
    std::vector<double> final;
    if (!profile(infer,state,instructions,disputed,2,&final,error)) return false;
    const size_t adjudicated=indices[winner(final)];
    result.probabilities=adjudicated==selected ? a : b;
    selected=adjudicated;
  }
  result.choice=candidates[selected].name;
  result.confidence=distribution_confidence(result.probabilities);
  *output=std::move(result);return true;
}

bool score(const PairInference& infer, const std::string& state,
           const std::string& instructions, const std::vector<std::string>& levels,
           ScoreResult* output, std::string* error) {
  if (!output) return fail(error,"invalid_evidence_output");
  if (levels.size()<2 || levels.size()>128) return fail(error,"invalid_evidence_input");
  std::unordered_set<std::string> unique;std::vector<TextPair> pairs;
  for (const auto& level : levels) {
    if (!unique.insert(level).second) return fail(error,"duplicate_score_level");
    std::string hypothesis;
    if (!question(instructions,level,&hypothesis,error)) return false;
    if (pairs.empty() && hypothesis=="The answer to the question \""+instructions+"\" is {}.")
      return fail(error,"invalid_question_template");
    pairs.push_back({state,hypothesis});
  }
  ScoreResult result;
  if (!distribution(infer,pairs,&result.probabilities,error)) return false;
  for (size_t i=0;i<levels.size();++i) result.score+=i*result.probabilities[i];
  result.confidence=distribution_confidence(result.probabilities);
  *output=std::move(result);return true;
}

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

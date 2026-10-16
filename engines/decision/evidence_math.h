#ifndef SAMOSA_DECISION_EVIDENCE_MATH_H
#define SAMOSA_DECISION_EVIDENCE_MATH_H
#include <algorithm>
#include <cmath>
#include <vector>
namespace samosa::decision {
inline double distribution_confidence(const std::vector<double>& probabilities) {
  if (probabilities.size() <= 1) return 1;
  double entropy = 0;
  for (double p : probabilities) if (p > 0) entropy -= p * std::log(p);
  return std::clamp(1 - entropy / std::log(probabilities.size()), 0.0, 1.0);
}
}
#endif

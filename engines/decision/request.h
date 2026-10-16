#ifndef SAMOSA_DECISION_TYPED_REQUEST_H
#define SAMOSA_DECISION_TYPED_REQUEST_H
#include "text_evidence.h"
namespace samosa::decision {
// Strict bounded JSON -> existing typed result JSON. The callback owns model
// execution/control checks. Failures preserve the caller's previous output.
bool execute_request(const PairInference&, const std::string& json,
                     std::string* output, std::string* error);
}
#endif

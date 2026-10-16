#include "decision/text_evidence.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace samosa::decision;
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::exit(1); } } while (0)
int main() {
  std::string error;
  double value = -1;
  size_t calls = 0;
  PairInference single = [&](const auto& pairs, auto truncation, auto* logits, auto*) {
    CHECK(pairs.size() == 1 && truncation == PairTruncation::LongestFirst);
    CHECK(pairs[0].premise == "state");
    logits->push_back({pairs[0].hypothesis == "true" ? 2.0f : -2.0f, 0.0f});
    ++calls; return true;
  };
  CHECK(predicate(single,"state","true",&value,&error));
  CHECK(std::abs(value - 0.8807970779778823) < 1e-7 && calls == 1);
  CHECK(binary_predicate(single,"state","true","false",&value,&error));
  CHECK(std::abs(value - 0.9820137900379085) < 1e-7 && calls == 3);
  std::vector<RelationInput> items(9, {"state","proposition","opposite"});
  calls = 0;
  PairInference batch = [&](const auto& pairs, auto truncation, auto* logits, auto*) {
    CHECK(truncation == PairTruncation::OnlyFirst);
    CHECK(pairs.size() == (calls < 2 ? 8u : 2u));
    for (size_t i = 0; i < pairs.size(); ++i) {
      CHECK(pairs[i].hypothesis == (i % 2 ? "opposite" : "proposition"));
      // Exercise all four threshold outcomes and exact equality at 0.5.
      const size_t item = calls * 4 + i/2;
      const bool positive = i % 2 ? (item % 4 >= 2) : (item % 4 == 0 || item % 4 == 2);
      logits->push_back({positive ? 0.0f : -2.0f, 0.0f});
    }
    ++calls; return true;
  };
  std::vector<RelationResult> result;
  CHECK(relations(batch,items,0.5,&result,&error));
  CHECK(result.size() == 9 && calls == 3);
  const char* expected[] = {"supports","unknown","conflicted","contradicts"};
  for (size_t i=0; i<result.size(); ++i) CHECK(result[i].relation == expected[i%4]);
  CHECK(relations({}, {}, 0.5, &result, &error) && result.empty());
  result.push_back({"sentinel",1,2,0.5});
  for (double threshold : {0.0, 1.0, std::numeric_limits<double>::quiet_NaN()}) {
    CHECK(!relations(batch,items,threshold,&result,&error));
    CHECK(result.size()==1 && result[0].relation=="sentinel");
  }
  for (int mode=0;mode<3;++mode) {
    PairInference bad = [mode](const auto&, auto, auto* logits, auto*) {
      if (mode==1) logits->push_back({std::numeric_limits<float>::infinity(),0});
      return mode!=2;
    };
    value = -1;
    CHECK(!predicate(bad,"state","true",&value,&error) && value == -1);
    CHECK(!relations(bad,items,0.5,&result,&error));
    CHECK(result.size()==1 && result[0].relation=="sentinel");
  }
  CHECK(!predicate(single,"state","true",nullptr,&error));
  CHECK(!relations(batch,items,0.5,nullptr,&error));
  std::printf("PASS native text evidence: %u checks; predicate semantics, batch order, four relations and atomic refusal\n",checks);
}

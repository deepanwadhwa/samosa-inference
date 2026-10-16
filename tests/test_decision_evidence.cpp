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
  const std::vector<Candidate> candidates={{"alpha_one","Same"},{"beta_two","Same"},{"gamma",""}};
  calls=0;
  PairInference disputed=[&](const auto& pairs,auto truncation,auto* logits,auto*) {
    CHECK(truncation==PairTruncation::OnlyFirst);
    if(calls==0) {
      CHECK(pairs.size()==3 && pairs[0].premise=="state");
      CHECK(pairs[0].hypothesis=="The answer to the question \"Question\" is Same.");
      CHECK(pairs[1].hypothesis=="The answer to the question \"Question\" is beta two: Same.");
      *logits={{0,0},{-1,0},{3,0}};
    } else if(calls==1) {
      CHECK(pairs.size()==3 && pairs[0].premise=="state");
      CHECK(pairs[0].hypothesis=="This example is alpha one: Same.");
      *logits={{4,0},{0,0},{0,0}};
    } else {
      CHECK(calls==2 && pairs.size()==2);
      CHECK(pairs[0].premise=="state\n\nQuestion: Question");
      CHECK(pairs[0].hypothesis=="This example is alpha one.");
      CHECK(pairs[1].hypothesis=="This example is gamma.");
      *logits={{2,0},{1,0}};
    }
    ++calls;return true;
  };
  ChoiceResult selected;
  CHECK(choice(disputed,"state","Question",candidates,&selected,&error));
  CHECK(calls==3 && selected.choice=="alpha_one" && selected.probabilities.size()==3);
  // Return the winning original profile, not the adjudicator's two-way scores.
  CHECK(selected.probabilities[0]>0.96 && selected.probabilities[1]<0.02);
  calls=0;
  PairInference uniform=[&](const auto& pairs,auto,auto* logits,auto*) {
    logits->assign(pairs.size(),{0,0});++calls;return true;
  };
  CHECK(choice(uniform,"state","Question",candidates,&selected,&error));
  CHECK(calls==2 && selected.choice=="alpha_one" && selected.confidence<1e-6);
  for(double p:selected.probabilities) CHECK(std::abs(p-1.0/3)<1e-7);
  ScoreResult scale;
  CHECK(score(uniform,"state","Question",{"low","middle","high"},&scale,&error));
  CHECK(std::abs(scale.score-1)<1e-7 && scale.confidence<1e-6);
  PairInference escaped=[&](const auto& pairs,auto,auto* logits,auto*) {
    CHECK(pairs[0].hypothesis=="The answer to the question \"Read {value}\" is low.");
    logits->assign(pairs.size(),{0,0});return true;
  };
  CHECK(score(escaped,"state","Read {{value}}",{"low","high"},&scale,&error));
  scale.score=123;
  CHECK(!score(uniform,"state","Read {value}",{"low","high"},&scale,&error) && scale.score==123);
  CHECK(!score(uniform,"state","Question",{"low","low"},&scale,&error) && scale.score==123);
  CHECK(!score(uniform,"state","Question",{"{}","high"},&scale,&error) && scale.score==123);
  CHECK(score(uniform,"state","Question",{"low","{}"},&scale,&error));
  selected.choice="sentinel";
  CHECK(!choice(uniform,"state","Question",{{"same","a"},{"same","b"}},&selected,&error));
  CHECK(selected.choice=="sentinel");
  calls=0;
  PairInference interrupted=[&](const auto& pairs,auto truncation,auto* logits,auto* why) {
    if(calls==2) return false;
    return disputed(pairs,truncation,logits,why);
  };
  CHECK(!choice(interrupted,"state","Question",candidates,&selected,&error));
  CHECK(calls==2 && selected.choice=="sentinel");
  std::printf("PASS native text scoring: %u checks; predicate, Choice profiles, ordered Score, relations and atomic refusal\n",checks);
}

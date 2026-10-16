// Postprocessing parity on pinned actual-model captures, not a provider test.
#include "decision/text_evidence.h"
#include "json.h"
#include "sha256.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace samosa::decision;
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) throw std::runtime_error(#x); } while (0)
static jval* get(jval* value, const char* name, jtype type) {
  jval* result=json_get(value,name);CHECK(result&&result->t==type);return result;
}
static std::string text(jval* value,const char* name) {return get(value,name,J_STR)->str;}
int main(int argc,char** argv) {
  try {
    CHECK(argc==3);
    std::ifstream input(argv[1],std::ios::binary|std::ios::ate);
    CHECK(input.good()&&input.tellg()>0&&input.tellg()<=2097152);
    input.seekg(0);std::ostringstream buffer;buffer<<input.rdbuf();
    const std::string bytes=buffer.str();Sha256 sha;unsigned char digest[32];char hex[65];
    sha256_init(&sha);sha256_update(&sha,bytes.data(),bytes.size());sha256_final(&sha,digest);sha256_hex(digest,hex);
    CHECK(std::string(hex)==argv[2]); // Only the authenticated QA report reaches the small internal parser.
    char* arena=nullptr;jval* report=json_parse(bytes.c_str(),&arena);
    struct Cleanup {jval* value;char* arena;~Cleanup(){json_free(value);free(arena);}} cleanup{report,arena};
    CHECK(text(report,"format")=="samosa.inference.opendecision-reference.v1");
    jval* cases=get(report,"cases",J_ARR);unsigned exercised=0;
    for(int i=0;i<cases->len;++i) {
      jval* test=cases->kids[i];const std::string method=text(test,"method");
      if(method!="noul"&&method!="relation"&&method!="relations"&&method!="choice"&&method!="score") continue;
      jval* args=get(test,"arguments",J_OBJ);jval* state=json_get(args,"state");
      if(method!="relations"&&(!state||state->t!=J_STR)) continue;
      jval* batches=get(test,"batches",J_ARR);size_t next=0;
      PairInference infer=[&](const auto& pairs,auto truncation,auto* output,auto*) {
        CHECK(truncation==(method=="noul"?PairTruncation::LongestFirst:PairTruncation::OnlyFirst));
        CHECK(next<static_cast<size_t>(batches->len));
        jval* logits=get(batches->kids[next++],"logits",J_ARR);
        CHECK(static_cast<size_t>(logits->len)==pairs.size());
        for(int row=0;row<logits->len;++row) {
          jval* values=logits->kids[row];CHECK(values->t==J_ARR&&values->len==2);
          CHECK(values->kids[0]->t==J_NUM&&values->kids[1]->t==J_NUM);
          output->push_back({static_cast<float>(values->kids[0]->num),static_cast<float>(values->kids[1]->num)});
        }
        return true;
      };
      std::string error;jval* expected=json_get(test,"result");CHECK(expected);
      if(method=="choice"||method=="score") {
        std::vector<double> probabilities;double certainty;
        if(method=="choice") {
          jval* criteria=get(args,"criteria",J_OBJ);std::vector<Candidate> candidates;
          for(int index=0;index<criteria->len;++index) {
            jval* description=criteria->kids[index];CHECK(description->t==J_STR||description->t==J_NULL);
            candidates.push_back({criteria->keys[index],description->t==J_STR?description->str:""});
          }
          ChoiceResult result;
          CHECK(choice(infer,state->str,text(args,"instructions"),candidates,&result,&error));
          CHECK(result.choice==text(expected,"choice"));
          probabilities=result.probabilities;certainty=result.confidence;
          jval* expected_probabilities=get(expected,"probabilities",J_OBJ);
          for(size_t index=0;index<candidates.size();++index)
            CHECK(std::abs(probabilities[index]-get(expected_probabilities,candidates[index].name.c_str(),J_NUM)->num)<1e-7);
        } else {
          jval* criteria=get(args,"criteria",J_ARR);std::vector<std::string> levels;
          for(int index=0;index<criteria->len;++index) { CHECK(criteria->kids[index]->t==J_STR);levels.push_back(criteria->kids[index]->str); }
          ScoreResult result;
          CHECK(score(infer,state->str,text(args,"instructions"),levels,&result,&error));
          CHECK(std::abs(result.score-get(expected,"score",J_NUM)->num)<1e-7);
          probabilities=result.probabilities;certainty=result.confidence;
          jval* expected_probabilities=get(expected,"probabilities",J_OBJ);
          for(size_t index=0;index<levels.size();++index)
            CHECK(std::abs(probabilities[index]-get(expected_probabilities,std::to_string(index).c_str(),J_NUM)->num)<1e-7);
        }
        CHECK(std::abs(certainty-get(expected,"confidence",J_NUM)->num)<1e-7);
      } else if(method=="noul") {
        double value=-1;jval* criteria=json_get(args,"criteria");
        if(criteria&&criteria->t==J_OBJ)
          CHECK(binary_predicate(infer,state->str,text(criteria,"true"),text(criteria,"false"),&value,&error));
        else CHECK(predicate(infer,state->str,text(args,"instructions"),&value,&error));
        CHECK(std::abs(value-get(expected,"noul",J_NUM)->num)<1e-7);
      } else {
        std::vector<RelationInput> items;
        if(method=="relation") items.push_back({state->str,text(args,"proposition"),text(args,"contradiction")});
        else {
          jval* inputs=get(args,"items",J_ARR);
          for(int index=0;index<inputs->len;++index) {
            jval* item=inputs->kids[index];items.push_back({text(item,"state"),text(item,"proposition"),text(item,"contradiction")});
          }
        }
        std::vector<RelationResult> actual;
        CHECK(relations(infer,items,get(args,"threshold",J_NUM)->num,&actual,&error));
        CHECK(actual.size()==items.size());
        for(size_t index=0;index<actual.size();++index) {
          jval* result=method=="relation"?expected:expected->kids[index];
          CHECK(actual[index].relation==text(result,"relation"));
          CHECK(text(result,"backend")=="explicit_opposite");
          jval* scores=get(result,"scores",J_OBJ);
          CHECK(std::abs(actual[index].supports-get(scores,"supports",J_NUM)->num)<1e-7);
          CHECK(std::abs(actual[index].contradicts-get(scores,"contradicts",J_NUM)->num)<1e-7);
        }
      }
      CHECK(next==static_cast<size_t>(batches->len));++exercised;
    }
    CHECK(exercised==9);
    std::cout<<"PASS native text evidence reference: "<<checks<<" checks; "<<exercised<<" actual-model captures, absolute probability tolerance 1e-7\n";
  } catch(const std::exception& error) {std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}

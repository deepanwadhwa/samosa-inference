// Postprocessing parity on pinned actual-model captures, not a provider test.
#include "decision/text_evidence.h"
#include "json.h"
#include "sha256.h"
#ifdef SAMOSA_DECISION_NATIVE_REAL
#include "decision/native_text.h"
#ifdef SAMOSA_DECISION_JSON_REAL
#include "decision/request.h"
#endif
#endif
#include <cmath>
#include <charconv>
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
#ifdef SAMOSA_DECISION_NATIVE_REAL
// Serialize authenticated QA fixtures, not public request input. The production
// request decoder is the Rust implementation exercised by the JSON-real mode.
#include "decision_reference_fixture.h"

#ifdef SAMOSA_DECISION_JSON_REAL
template<class Close>
static void compare_result(jval* actual,jval* expected,const Close& close) {
  CHECK(actual&&expected&&actual->t==expected->t);
  if(expected->t==J_NUM) {CHECK(close(actual->num,expected->num));}
  else if(expected->t==J_STR) {CHECK(std::string(actual->str)==expected->str);}
  else if(expected->t==J_BOOL) {CHECK(actual->boolean==expected->boolean);}
  else if(expected->t==J_ARR||expected->t==J_OBJ) {
    CHECK(actual->len==expected->len);
    for(int i=0;i<expected->len;++i)
      compare_result(expected->t==J_OBJ?json_get(actual,expected->keys[i]):actual->kids[i],expected->kids[i],close);
  }
}
#endif
static void verify_file(const char* path,const char* expected) {
  std::ifstream input(path,std::ios::binary);CHECK(input.good());Sha256 sha;sha256_init(&sha);
  char block[65536];while(input) {input.read(block,sizeof(block));sha256_update(&sha,block,static_cast<size_t>(input.gcount()));}
  CHECK(input.eof());unsigned char digest[32];char hex[65];sha256_final(&sha,digest);sha256_hex(digest,hex);
  CHECK(std::string(hex)==expected);
}
static void compare_tokens(jval* inputs,const char* key,const std::vector<int64_t>& actual,
                           size_t rows,size_t columns) {
  jval* matrix=get(inputs,key,J_ARR);CHECK(static_cast<size_t>(matrix->len)==rows);
  for(size_t row=0;row<rows;++row) {
    jval* values=matrix->kids[row];CHECK(values->t==J_ARR&&static_cast<size_t>(values->len)==columns);
    for(size_t col=0;col<columns;++col) CHECK(values->kids[col]->num==actual[row*columns+col]);
  }
}
#endif
int main(int argc,char** argv) {
  try {
#ifdef SAMOSA_DECISION_NATIVE_REAL
    CHECK(argc==7); // MODEL TOKENIZER REPORT MODEL_SHA TOKENIZER_SHA REPORT_SHA
    verify_file(argv[1],argv[4]);verify_file(argv[2],argv[5]);
    std::ifstream tokenizer_input(argv[2],std::ios::binary|std::ios::ate);
    CHECK(tokenizer_input.good()&&tokenizer_input.tellg()>0&&tokenizer_input.tellg()<=16777216);
    tokenizer_input.seekg(0);std::ostringstream tokenizer_bytes;tokenizer_bytes<<tokenizer_input.rdbuf();
    NativeText native;std::string load_error;
    CHECK(native.load(argv[1],tokenizer_bytes.str(),&load_error));
    std::string rendered="sentinel";
    CHECK(NativeText::render_state("\"hello\\u0000world\"",&rendered,&load_error));
    CHECK(rendered==std::string("hello\0world",11));
    CHECK(!NativeText::render_state("{",&rendered,&load_error));
    CHECK(rendered==std::string("hello\0world",11));
    std::vector<std::array<float,2>> sentinel{{123,456}};EncodedPairs observed;observed.rows=123;
    NativeText unloaded;
    CHECK(!unloaded.infer({{"state","statement"}},PairTruncation::OnlyFirst,&sentinel,&load_error,&observed));
    CHECK(sentinel[0][0]==123&&observed.rows==123);
    CHECK(!native.infer({},PairTruncation::OnlyFirst,&sentinel,&load_error,&observed));
    CHECK(!native.infer(std::vector<TextPair>(9),PairTruncation::OnlyFirst,&sentinel,&load_error,&observed));
    CHECK(!native.infer({{std::string(1,static_cast<char>(0xff)),"statement"}},PairTruncation::OnlyFirst,&sentinel,&load_error,&observed));
    CHECK(sentinel[0][0]==123&&observed.rows==123);
    CHECK(!native.load(argv[1],"{}",&load_error));
    CHECK(!native.load("/samosa-missing-decision-model",tokenizer_bytes.str(),&load_error));
    // All subsequent real decisions must still use the previous valid model.
    const char* report_path=argv[3];const char* report_pin=argv[6];
    constexpr double tolerance=0.005;
    double maximum_logit_error=0;
#else
    CHECK(argc==3);
    const char* report_path=argv[1];const char* report_pin=argv[2];
    constexpr double tolerance=1e-7;
#endif
    double maximum_result_error=0;
    const auto close=[&](double actual,double expected) {
      const double difference=std::abs(actual-expected);
      maximum_result_error=std::max(maximum_result_error,difference);
      return difference<tolerance;
    };
    std::ifstream input(report_path,std::ios::binary|std::ios::ate);
    CHECK(input.good()&&input.tellg()>0&&input.tellg()<=2097152);
    input.seekg(0);std::ostringstream buffer;buffer<<input.rdbuf();
    const std::string bytes=buffer.str();Sha256 sha;unsigned char digest[32];char hex[65];
    sha256_init(&sha);sha256_update(&sha,bytes.data(),bytes.size());sha256_final(&sha,digest);sha256_hex(digest,hex);
    CHECK(std::string(hex)==report_pin); // Only the authenticated QA report reaches the small internal parser.
    char* arena=nullptr;jval* report=json_parse(bytes.c_str(),&arena);
    struct Cleanup {jval* value;char* arena;~Cleanup(){json_free(value);free(arena);}} cleanup{report,arena};
    CHECK(text(report,"format")=="samosa.inference.opendecision-reference.v1");
    jval* cases=get(report,"cases",J_ARR);unsigned exercised=0;
    for(int i=0;i<cases->len;++i) {
      jval* test=cases->kids[i];const std::string method=text(test,"method");
      if(method!="noul"&&method!="relation"&&method!="relations"&&method!="choice"&&method!="score") continue;
      jval* args=get(test,"arguments",J_OBJ);jval* state=json_get(args,"state");
      std::string state_text;
      if(method!="relations") {
#ifdef SAMOSA_DECISION_NATIVE_REAL
        if(state&&state->t==J_OBJ) {
          CHECK(NativeText::render_state(fixture_json(state),&state_text,&load_error));
        } else {CHECK(state&&state->t==J_STR);state_text=state->str;}
#else
        if(!state||state->t!=J_STR) continue;
        state_text=state->str;
#endif
      }
      std::cout<<"BEGIN "<<text(test,"name")<<std::endl;
      jval* batches=get(test,"batches",J_ARR);size_t next=0;
      PairInference infer=[&](const auto& pairs,auto truncation,auto* output,auto*) {
        CHECK(truncation==(method=="noul"?PairTruncation::LongestFirst:PairTruncation::OnlyFirst));
        CHECK(next<static_cast<size_t>(batches->len));
        jval* batch=batches->kids[next++];jval* logits=get(batch,"logits",J_ARR);
        CHECK(static_cast<size_t>(logits->len)==pairs.size());
#ifdef SAMOSA_DECISION_NATIVE_REAL
        EncodedPairs tensors;std::string native_error;
        CHECK(native.infer(pairs,truncation,output,&native_error,&tensors));
        jval* inputs=get(batch,"inputs",J_OBJ);
        compare_tokens(inputs,"input_ids",tensors.ids,tensors.rows,tensors.columns);
        compare_tokens(inputs,"attention_mask",tensors.mask,tensors.rows,tensors.columns);
        if(json_get(inputs,"token_type_ids")) compare_tokens(inputs,"token_type_ids",tensors.types,tensors.rows,tensors.columns);
#endif
        for(int row=0;row<logits->len;++row) {
          jval* values=logits->kids[row];CHECK(values->t==J_ARR&&values->len==2);
          CHECK(values->kids[0]->t==J_NUM&&values->kids[1]->t==J_NUM);
#ifdef SAMOSA_DECISION_NATIVE_REAL
          for(size_t col=0;col<2;++col) {
            const double difference=std::abs((*output)[row][col]-values->kids[col]->num);
            CHECK(difference<=0.01);maximum_logit_error=std::max(maximum_logit_error,difference);
          }
#else
          output->push_back({static_cast<float>(values->kids[0]->num),static_cast<float>(values->kids[1]->num)});
#endif
        }
        return true;
      };
      std::string error;jval* expected=json_get(test,"result");CHECK(expected);
#ifdef SAMOSA_DECISION_JSON_REAL
      const std::string arguments=fixture_json(args);
      CHECK(!arguments.empty()&&arguments[0]=='{');
      const std::string request="{\"method\":\""+method+"\","+arguments.substr(1);
      std::string response="sentinel";
      if(!execute_request(infer,request,&response,&error)) throw std::runtime_error(error);
      char* response_arena=nullptr;jval* result=json_parse(response.c_str(),&response_arena);
      struct ResultCleanup {jval* value;char* arena;~ResultCleanup(){json_free(value);free(arena);}} result_cleanup{result,response_arena};
      compare_result(result,expected,close);
#else
      if(method=="choice"||method=="score") {
        std::vector<double> probabilities;double certainty;
        if(method=="choice") {
          jval* criteria=get(args,"criteria",J_OBJ);std::vector<Candidate> candidates;
          for(int index=0;index<criteria->len;++index) {
            jval* description=criteria->kids[index];CHECK(description->t==J_STR||description->t==J_NULL);
            candidates.push_back({criteria->keys[index],description->t==J_STR?description->str:""});
          }
          ChoiceResult result;
          CHECK(choice(infer,state_text,text(args,"instructions"),candidates,&result,&error));
          CHECK(result.choice==text(expected,"choice"));
          probabilities=result.probabilities;certainty=result.confidence;
          jval* expected_probabilities=get(expected,"probabilities",J_OBJ);
          for(size_t index=0;index<candidates.size();++index)
            CHECK(close(probabilities[index],get(expected_probabilities,candidates[index].name.c_str(),J_NUM)->num));
        } else {
          jval* criteria=get(args,"criteria",J_ARR);std::vector<std::string> levels;
          for(int index=0;index<criteria->len;++index) { CHECK(criteria->kids[index]->t==J_STR);levels.push_back(criteria->kids[index]->str); }
          ScoreResult result;
          CHECK(score(infer,state_text,text(args,"instructions"),levels,&result,&error));
          CHECK(close(result.score,get(expected,"score",J_NUM)->num));
          probabilities=result.probabilities;certainty=result.confidence;
          jval* expected_probabilities=get(expected,"probabilities",J_OBJ);
          for(size_t index=0;index<levels.size();++index)
            CHECK(close(probabilities[index],get(expected_probabilities,std::to_string(index).c_str(),J_NUM)->num));
        }
        CHECK(close(certainty,get(expected,"confidence",J_NUM)->num));
      } else if(method=="noul") {
        double value=-1;jval* criteria=json_get(args,"criteria");
        if(criteria&&criteria->t==J_OBJ)
          CHECK(binary_predicate(infer,state_text,text(criteria,"true"),text(criteria,"false"),&value,&error));
        else CHECK(predicate(infer,state_text,text(args,"instructions"),&value,&error));
        CHECK(close(value,get(expected,"noul",J_NUM)->num));
      } else {
        std::vector<RelationInput> items;
        if(method=="relation") items.push_back({state_text,text(args,"proposition"),text(args,"contradiction")});
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
          CHECK(close(actual[index].supports,get(scores,"supports",J_NUM)->num));
          CHECK(close(actual[index].contradicts,get(scores,"contradicts",J_NUM)->num));
        }
      }
#endif
      CHECK(next==static_cast<size_t>(batches->len));++exercised;
    }
#ifdef SAMOSA_DECISION_NATIVE_REAL
    CHECK(exercised==10);
    std::cout<<"Native model maximum absolute logit error: "<<maximum_logit_error<<"\n";
#else
    CHECK(exercised==9);
#endif
    std::cout<<"Maximum absolute result error: "<<maximum_result_error<<"\n";
    std::cout<<"PASS native text evidence reference: "<<checks<<" checks; "<<exercised<<" actual-model captures, absolute result tolerance "<<tolerance<<"\n";
  } catch(const std::exception& error) {std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}

#include "native_image.h"
#include "evidence_math.h"
#include "token_json.h"
#include "tokenizer/tokenizer.h"
#include <algorithm>
#include <cmath>

namespace samosa::decision {
namespace {
bool fail(std::string* error, const char* message) {if(error)*error=message;return false;}
double sigmoid(float value) {
  return static_cast<double>(1.0f / (1.0f + std::exp(-value)));
}
bool distribution(NativeImage& model,const std::vector<float>& pixels,
                  const std::vector<std::string>& descriptions,std::vector<double>* output,
                  std::string* error) {
  std::vector<float> logits;
  if(!model.infer(descriptions,pixels,&logits,error)) return false;
  const float maximum=*std::max_element(logits.begin(),logits.end());
  float total=0;
  for(auto& value:logits) {value=std::exp(value-maximum);total+=value;}
  if(!std::isfinite(total)||total<=0) return fail(error,"invalid_image_distribution");
  std::vector<double> result;
  for(float value:logits) result.push_back(static_cast<double>(value/total));
  *output=std::move(result);return true;
}
}
NativeImage::~NativeImage() {sm_tokenizer_free(tokenizer_);}
bool NativeImage::load(const std::string& model,const std::string& tokenizer,std::string* error) {
  void* replacement=nullptr;
  if(sm_tokenizer_load(reinterpret_cast<const uint8_t*>(tokenizer.data()),tokenizer.size(),&replacement))
    return fail(error,"invalid_decision_tokenizer");
  if(!model_.load(model,error)) {sm_tokenizer_free(replacement);return false;}
  sm_tokenizer_free(tokenizer_);tokenizer_=replacement;return true;
}
bool NativeImage::infer(const std::vector<std::string>& descriptions,const std::vector<float>& pixels,
                        std::vector<float>* output,std::string* error) {
  if(!output||!tokenizer_||descriptions.empty()||descriptions.size()>128)
    return fail(error,"invalid_image_descriptions");
  size_t total=0;
  for(const auto& description:descriptions) {
    total+=description.size();
    if(description.size()>65536||total>524288) return fail(error,"decision_input_limit");
  }
  std::vector<float> all;
  for(size_t start=0;start<descriptions.size();start+=8) {
    size_t end=std::min(start+8,descriptions.size()),rows=end-start;
    std::string request="{\"texts\":[";
    for(size_t index=start;index<end;++index) {
      if(index!=start) request+=',';
      request+='[';token_json::quote(descriptions[index],&request);request+=']';
    }
    request+="],\"max_length\":64,\"padding\":\"max_length\",\"truncation\":\"longest_first\","
             "\"padding_side\":\"right\",\"truncation_side\":\"right\",\"pad_token\":\"<pad>\",\"pad_id\":0,\"pad_type_id\":0}";
    SmTokenizerOutput encoded={};
    if(sm_tokenizer_encode(tokenizer_,reinterpret_cast<const uint8_t*>(request.data()),request.size(),&encoded))
      return fail(error,"decision_tokenization_failed");
    struct Release {SmTokenizerOutput* value;~Release(){sm_tokenizer_output_free(value);}} release{&encoded};
    const std::string bytes(reinterpret_cast<const char*>(encoded.data),encoded.length);
    char* arena=nullptr;jval* parsed=json_parse(bytes.c_str(),&arena);
    struct Cleanup {jval* value;char* arena;~Cleanup(){json_free(value);free(arena);}} cleanup{parsed,arena};
    size_t columns=0;std::vector<int64_t> ids,mask,types;
    if(!token_json::matrix(json_get(parsed,"input_ids"),rows,&columns,255999,&ids)||columns!=64||
       !token_json::matrix(json_get(parsed,"attention_mask"),rows,&columns,1,&mask)||
       !token_json::matrix(json_get(parsed,"token_type_ids"),rows,&columns,0,&types))
      return fail(error,"invalid_decision_tokens");
    std::vector<float> logits;
    if(!model_.infer(ids,pixels,rows,&logits,error)) return false;
    all.insert(all.end(),logits.begin(),logits.end());
  }
  *output=std::move(all);return true;
}
DecisionBackend image_backend(NativeImage& model,const std::vector<float>& pixels) {
  DecisionBackend backend;
  backend.relation_backend="siglip_image_text";
  backend.choose=[&](const auto&,const auto&,const std::vector<Candidate>& candidates,ChoiceResult* output,std::string* error) {
    if(!output||candidates.size()<2||candidates.size()>128) return fail(error,"invalid_image_choice");
    std::vector<std::string> descriptions;
    for(const auto& candidate:candidates) {
      auto description=candidate.description.empty()?candidate.name:candidate.description;
      if(candidate.description.empty()) std::replace(description.begin(),description.end(),'_',' ');
      descriptions.push_back(std::move(description));
    }
    ChoiceResult result;
    if(!distribution(model,pixels,descriptions,&result.probabilities,error)) return false;
    size_t selected=std::max_element(result.probabilities.begin(),result.probabilities.end())-result.probabilities.begin();
    result.choice=candidates[selected].name;result.confidence=distribution_confidence(result.probabilities);
    *output=std::move(result);return true;
  };
  backend.scale=[&](const auto&,const auto&,const std::vector<std::string>& levels,ScoreResult* output,std::string* error) {
    if(!output||levels.size()<2) return fail(error,"invalid_image_score");
    ScoreResult result;
    if(!distribution(model,pixels,levels,&result.probabilities,error)) return false;
    for(size_t index=0;index<levels.size();++index) result.score+=index*result.probabilities[index];
    result.confidence=distribution_confidence(result.probabilities);*output=std::move(result);return true;
  };
  backend.predicate=[&](const auto&,const std::string& statement,double* output,std::string* error) {
    std::vector<float> logits;
    if(!output||!model.infer({statement},pixels,&logits,error)) return false;
    *output=sigmoid(logits[0]);return true;
  };
  backend.binary=[&](const auto&,const std::string& yes,const std::string& no,double* output,std::string* error) {
    std::vector<double> probabilities;
    if(!output||!distribution(model,pixels,{yes,no},&probabilities,error)) return false;
    *output=probabilities[0];return true;
  };
  backend.relate=[&](const std::vector<RelationInput>& items,double threshold,std::vector<RelationResult>* output,std::string* error) {
    // Original image API has one frame and no batch-relation method.
    if(!output||items.size()!=1||!std::isfinite(threshold)||threshold<=0||threshold>=1)
      return fail(error,"invalid_image_relation");
    std::vector<float> logits;
    if(!model.infer({items[0].proposition,items[0].contradiction},pixels,&logits,error)) return false;
    RelationResult result;result.supports=sigmoid(logits[0]);result.contradicts=sigmoid(logits[1]);result.threshold=threshold;
    result.relation=result.supports>=threshold?(result.contradicts>=threshold?"conflicted":"supports"):
        (result.contradicts>=threshold?"contradicts":"unknown");
    *output={result};return true;
  };
  return backend;
}
}

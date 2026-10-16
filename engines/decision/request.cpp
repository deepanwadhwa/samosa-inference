#include "request.h"
#include "tokenizer/request.h"
#include <charconv>
#include <stdexcept>
namespace samosa::decision {
namespace {
std::string text(SmDecisionText value) {
  if(!value.length) return {};
  return std::string(reinterpret_cast<const char*>(value.data),value.length);
}
std::string quote(const std::string& value) {
  constexpr char hex[]="0123456789abcdef";std::string output="\"";
  for(unsigned char c:value) {
    if(c=='"'||c=='\\') {output+='\\';output+=static_cast<char>(c);}
    else if(c<32) {output+="\\u00";output+=hex[c>>4];output+=hex[c&15];}
    else output+=static_cast<char>(c);
  }
  return output+'"';
}
std::string number(double value) {
  char bytes[64];const auto result=std::to_chars(bytes,bytes+sizeof(bytes),value);
  if(result.ec!=std::errc{}) throw std::runtime_error("decision_number_encoding_failed");
  return std::string(bytes,result.ptr);
}
std::string relation_json(const RelationResult& result) {
  return "{\"type\":\"relation\",\"relation\":"+quote(result.relation)+
      ",\"scores\":{\"supports\":"+number(result.supports)+",\"contradicts\":"+number(result.contradicts)+
      "},\"threshold\":"+number(result.threshold)+",\"backend\":\"explicit_opposite\"}";
}
bool fail(std::string* error,const char* message) {if(error)*error=message;return false;}
}
bool execute_request(const PairInference& infer,const std::string& json,
                     std::string* output,std::string* error) {
  if(!output) return fail(error,"invalid_decision_output");
  void* handle=nullptr;
  const int status=sm_decision_request_load(reinterpret_cast<const uint8_t*>(json.data()),json.size(),&handle);
  if(status) return fail(error,status==5?"decision_request_limit":"invalid_decision_request");
  struct Release {void* handle;~Release(){sm_decision_request_free(handle);}} release{handle};
  SmDecisionRequest view={};
  if(sm_decision_request_view(handle,&view)) return fail(error,"invalid_decision_request");
  const std::string state=text(view.state),instructions=text(view.instructions);
  std::string result;
  if(view.kind==SM_DECISION_CHOICE) {
    std::vector<Candidate> candidates;
    for(size_t i=0;i<view.candidate_count;++i) candidates.push_back({text(view.candidates[i].name),text(view.candidates[i].description)});
    ChoiceResult selected;
    if(!choice(infer,state,instructions,candidates,&selected,error)) return false;
    result="{\"type\":\"choice\",\"choice\":"+quote(selected.choice)+",\"probabilities\":{";
    for(size_t i=0;i<candidates.size();++i) {
      if(i) result+=',';
      result+=quote(candidates[i].name)+":"+number(selected.probabilities[i]);
    }
    result+="},\"confidence\":"+number(selected.confidence)+'}';
  } else if(view.kind==SM_DECISION_SCORE) {
    std::vector<std::string> levels;for(size_t i=0;i<view.level_count;++i) levels.push_back(text(view.levels[i]));
    ScoreResult scored;
    if(!score(infer,state,instructions,levels,&scored,error)) return false;
    result="{\"type\":\"score\",\"score\":"+number(scored.score)+",\"legend\":{";
    for(size_t i=0;i<levels.size();++i) {if(i)result+=',';result+=quote(std::to_string(i))+":"+quote(levels[i]);}
    result+="},\"probabilities\":{";
    for(size_t i=0;i<levels.size();++i) {if(i)result+=',';result+=quote(std::to_string(i))+":"+number(scored.probabilities[i]);}
    result+="},\"confidence\":"+number(scored.confidence)+'}';
  } else if(view.kind==SM_DECISION_NOUL||view.kind==SM_DECISION_BINARY) {
    double value;
    const bool success=view.kind==SM_DECISION_NOUL?predicate(infer,state,instructions,&value,error):
        binary_predicate(infer,state,text(view.levels[0]),text(view.levels[1]),&value,error);
    if(!success)return false;
    result="{\"type\":\"noul\",\"noul\":"+number(value)+'}';
  } else {
    std::vector<RelationInput> items;
    for(size_t i=0;i<view.relation_count;++i) {
      const auto& item=view.relations[i];items.push_back({text(item.state),text(item.proposition),text(item.contradiction)});
    }
    std::vector<RelationResult> related;
    if(!relations(infer,items,view.threshold,&related,error))return false;
    if(view.kind==SM_DECISION_RELATION) result=relation_json(related[0]);
    else {result="[";for(size_t i=0;i<related.size();++i){if(i)result+=',';result+=relation_json(related[i]);}result+=']';}
  }
  if(result.size()>1048576) return fail(error,"decision_result_limit");
  *output=std::move(result);return true;
}
}

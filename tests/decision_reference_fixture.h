#ifndef SAMOSA_DECISION_REFERENCE_FIXTURE_H
#define SAMOSA_DECISION_REFERENCE_FIXTURE_H
#include "json.h"
#include <charconv>
#include <string>
inline std::string fixture_json(jval* value) {
  CHECK(value);
  if(value->t==J_NULL)return "null";
  if(value->t==J_BOOL)return value->boolean?"true":"false";
  if(value->t==J_NUM) {
    char bytes[64];auto result=std::to_chars(bytes,bytes+sizeof(bytes),value->num);
    CHECK(result.ec==std::errc{});return std::string(bytes,result.ptr);
  }
  if(value->t==J_ARR) {
    std::string result="[";
    for(int i=0;i<value->len;++i){if(i)result+=',';result+=fixture_json(value->kids[i]);}
    return result+']';
  }
  if(value->t==J_STR) {
    std::string result="\"";for(const char* p=value->str;*p;++p) {
      CHECK(static_cast<unsigned char>(*p)>=32);
      if(*p=='"'||*p=='\\') result+='\\';
      result+=*p;
    }
    return result+'"';
  }
  std::string result="{";
  for(int i=0;i<value->len;++i) {
    if(i) result+=',';
    jval key={};key.t=J_STR;key.str=value->keys[i];
    result+=fixture_json(&key)+":"+fixture_json(value->kids[i]);
  }
  return result+'}';
}
#endif

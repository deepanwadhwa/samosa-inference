#ifndef SAMOSA_DECISION_TOKEN_JSON_H
#define SAMOSA_DECISION_TOKEN_JSON_H
#include "../json.h"
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
namespace samosa::decision::token_json {
inline void quote(const std::string& text, std::string* output) {
  static constexpr char hex[]="0123456789abcdef";
  *output+='"';
  for (unsigned char c:text) {
    if(c=='"'||c=='\\') { *output+='\\';*output+=static_cast<char>(c); }
    else if(c<32) { *output+="\\u00";*output+=hex[c>>4];*output+=hex[c&15]; }
    else *output+=static_cast<char>(c);
  }
  *output+='"';
}
inline bool matrix(jval* value, size_t rows, size_t* columns, int64_t maximum,
            std::vector<int64_t>* output) {
  if(!value||value->t!=J_ARR||value->len!=static_cast<int>(rows)) return false;
  for(int row=0;row<value->len;++row) {
    jval* items=value->kids[row];
    if(!items||items->t!=J_ARR||items->len<1||items->len>512) return false;
    if(!*columns) *columns=static_cast<size_t>(items->len);
    if(*columns!=static_cast<size_t>(items->len)) return false;
    for(int col=0;col<items->len;++col) {
      jval* item=items->kids[col];
      if(!item||item->t!=J_NUM||!std::isfinite(item->num)||item->num<0||
         item->num>maximum||std::floor(item->num)!=item->num) return false;
      output->push_back(static_cast<int64_t>(item->num));
    }
  }
  return true;
}
}
#endif

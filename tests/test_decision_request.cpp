#include "decision/request.h"
#include <cstdio>
#include <cstdlib>
using namespace samosa::decision;
static unsigned checks;
#define CHECK(x) do {++checks;if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
int main() {
  size_t calls=0;
  PairInference infer=[&](const auto& pairs,auto,auto* output,auto*) {
    CHECK(!pairs.empty()&&pairs.size()<=8);output->assign(pairs.size(),{0,0});++calls;return true;
  };
  std::string output,error;
  CHECK(execute_request(infer,R"({"method":"choice","state":"s","instructions":"q","criteria":{"z":null,"a":"A"}})",&output,&error));
  CHECK(output==R"({"type":"choice","choice":"z","probabilities":{"z":0.5,"a":0.5},"confidence":0})");
  CHECK(calls==2);
  CHECK(execute_request(infer,R"({"method":"score","state":"s","instructions":"q","criteria":["low","high"]})",&output,&error));
  CHECK(output==R"({"type":"score","score":0.5,"legend":{"0":"low","1":"high"},"probabilities":{"0":0.5,"1":0.5},"confidence":0})");
  CHECK(execute_request(infer,R"({"method":"noul","state":{},"instructions":"q"})",&output,&error));
  CHECK(output==R"({"type":"noul","noul":0.5})");
  const size_t before=calls;
  CHECK(execute_request(infer,R"({"method":"noul","state":{},"instructions":"unused","criteria":{"true":"yes","false":"no"}})",&output,&error));
  CHECK(calls==before+2&&output==R"({"type":"noul","noul":0.5})");
  CHECK(execute_request(infer,R"({"method":"relation","state":0,"proposition":"p","contradiction":"c"})",&output,&error));
  CHECK(output==R"({"type":"relation","relation":"conflicted","scores":{"supports":0.5,"contradicts":0.5},"threshold":0.5,"backend":"explicit_opposite"})");
  CHECK(execute_request(infer,R"({"method":"relations","items":[]})",&output,&error)&&output=="[]");
  PairInference nul=[&](const auto& pairs,auto,auto* logits,auto*) {
    CHECK(pairs.size()==1&&pairs[0].premise==std::string("hi\0x",4));
    CHECK(pairs[0].hypothesis=="question");logits->assign(1,{0,0});return true;
  };
  CHECK(execute_request(nul,R"({"method":"noul","state":"hi\u0000x","instructions":"question"})",&output,&error));
  PairInference structured=[&](const auto& pairs,auto,auto* logits,auto*) {
    CHECK(pairs[0].premise==R"({"a":1e-05,"z":true})");logits->assign(1,{0,0});return true;
  };
  CHECK(execute_request(structured,R"({"method":"noul","state":{"z":true,"a":1e-5},"instructions":"question"})",&output,&error));
  output="sentinel";
  for(const auto* bad:{R"({"method":"noul","state":0,"instructions":"q","extra":1})",
      R"({"method":"choice","state":0,"instructions":"q","criteria":{"a":"one","a":"two"}})","{"}) {
    CHECK(!execute_request(infer,bad,&output,&error)&&output=="sentinel");
  }
  PairInference failed=[](const auto&,auto,auto*,auto*){return false;};
  CHECK(!execute_request(failed,R"({"method":"noul","state":0,"instructions":"q"})",&output,&error)&&output=="sentinel");
  CHECK(!execute_request(infer,"{}",nullptr,&error));
  DecisionBackend image;
  CHECK(!execute_request(image,R"({"method":"noul","state":0,"instructions":"q"})",&output,&error)&&output=="sentinel");
  image.choose=[](const auto&,const auto&,const auto&,auto*,auto*){return false;};
  image.scale=[](const auto&,const auto&,const auto&,auto*,auto*){return false;};
  image.predicate=[](const auto&,const auto&,auto*,auto*){return false;};
  image.binary=[](const auto&,const auto&,const auto&,auto*,auto*){return false;};
  image.relate=[](const auto&,double threshold,auto* result,auto*) {
    *result={{"unknown",0.2,0.1,threshold}};return true;
  };
  image.relation_backend="siglip_image_text";
  CHECK(!execute_request(image,R"({"method":"relations","items":[]})",&output,&error)&&output=="sentinel"&&error=="unsupported_image_method");
  CHECK(execute_request(image,R"({"method":"relation","state":"frame","proposition":"p","contradiction":"c"})",&output,&error));
  CHECK(output==R"({"type":"relation","relation":"unknown","scores":{"supports":0.2,"contradicts":0.1},"threshold":0.5,"backend":"siglip_image_text"})");
  std::printf("PASS typed native requests: %u checks; ordered JSON, every operation, structured/NUL state and atomic refusal\n",checks);
}

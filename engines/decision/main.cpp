// Private persistent worker. The owning provider verifies/snapshots assets and
// applies process admission, deadlines and cancellation before launching it.
#include "native_text.h"
#include "request.h"
#include "transport.h"
#include <csignal>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unistd.h>
int main(int argc,char** argv) {
  try {
    std::string model,tokenizer;
    for(int i=1;i<argc;++i) {
      const std::string option=argv[i];
      if(option=="--model"&&model.empty()&&i+1<argc)model=argv[++i];
      else if(option=="--tokenizer"&&tokenizer.empty()&&i+1<argc)tokenizer=argv[++i];
      else return 2;
    }
    if(model.empty()||tokenizer.empty()||model[0]!='/'||tokenizer[0]!='/')return 2;
    std::signal(SIGPIPE,SIG_IGN);
    std::ifstream input(tokenizer,std::ios::binary|std::ios::ate);
    if(!input.good()||input.tellg()<=0||input.tellg()>16777216)return 3;
    input.seekg(0);std::ostringstream bytes;bytes<<input.rdbuf();
    if(!input.good())return 3;
    samosa::decision::NativeText native;std::string error;
    if(!native.load(model,bytes.str(),&error)){std::cerr<<"decision engine load failed\n";return 3;}
    samosa::decision::PairInference infer=[&](const auto& pairs,auto truncation,auto* logits,auto* why) {
      return native.infer(pairs,truncation,logits,why);
    };
    const auto status=samosa::decision::serve_requests(STDIN_FILENO,STDOUT_FILENO,
        [&](const auto& request,auto* reply,auto* why) {
          return samosa::decision::execute_request(infer,request,reply,why);
        });
    return status==samosa::decision::TransportResult::Eof?0:4;
  } catch(const std::exception&) {std::cerr<<"decision engine failed\n";return 5;}
}

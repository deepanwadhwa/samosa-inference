// Model-free transport fixture; never compiled into the production worker.
#include "decision/request.h"
#include "decision/transport.h"
#include <csignal>
#include <stdexcept>
#include <unistd.h>
int main() {
  std::signal(SIGPIPE,SIG_IGN);
  samosa::decision::PairInference infer=[](const auto& pairs,auto,auto* logits,auto*) {
    logits->assign(pairs.size(),{0,0});return true;
  };
  return static_cast<int>(samosa::decision::serve_requests(STDIN_FILENO,STDOUT_FILENO,
      [&](const auto& request,auto* reply,auto* error) {
        if(request=="throw")throw std::runtime_error("/private/model/location");
        if(request=="oversize"){reply->assign(1048577,'x');return true;}
        if(request=="empty"){reply->clear();return true;}
        return samosa::decision::execute_request(infer,request,reply,error);
      }));
}

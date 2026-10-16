#include "transport.h"
#include <cerrno>
#include <cstdint>
#include <unistd.h>
namespace samosa::decision {
namespace {
constexpr uint32_t limit=1048576;
enum class Read {Complete,Eof,Truncated,Failed};
Read read_exact(int fd,void* data,size_t size) {
  size_t done=0;auto* bytes=static_cast<unsigned char*>(data);
  while(done<size) {
    const ssize_t count=read(fd,bytes+done,size-done);
    if(count<0&&errno==EINTR)continue;
    if(count<0)return Read::Failed;
    if(!count)return done?Read::Truncated:Read::Eof;
    done+=static_cast<size_t>(count);
  }
  return Read::Complete;
}
bool write_exact(int fd,const void* data,size_t size) {
  size_t done=0;const auto* bytes=static_cast<const unsigned char*>(data);
  while(done<size) {
    const ssize_t count=write(fd,bytes+done,size-done);
    if(count<0&&errno==EINTR)continue;
    if(count<=0)return false;
    done+=static_cast<size_t>(count);
  }
  return true;
}
std::string error_reply(const std::string& error) {
  // Never forward native exceptions or asset paths to the wire. Only fixed
  // semantic codes may leave the engine; details belong in the owner's logs.
  for(const char* code:{"invalid_decision_request","decision_request_limit","decision_result_limit",
      "invalid_question_template","ambiguous_candidate_labels","context_limit"})
    if(error==code)return "{\"error\":\""+error+"\"}";
  return "{\"error\":\"engine_failed\"}";
}
}
TransportResult serve_requests(int input,int output,const RequestHandler& handler) {
  if(input<0||output<0||!handler)return TransportResult::IoFailure;
  for(;;) {
    unsigned char header[4];const Read opening=read_exact(input,header,sizeof(header));
    if(opening==Read::Eof)return TransportResult::Eof;
    if(opening==Read::Failed)return TransportResult::IoFailure;
    if(opening!=Read::Complete)return TransportResult::InvalidFrame;
    const uint32_t size=(uint32_t(header[0])<<24)|(uint32_t(header[1])<<16)|(uint32_t(header[2])<<8)|header[3];
    if(!size||size>limit)return TransportResult::InvalidFrame;
    std::string request(size,'\0');const Read body=read_exact(input,request.data(),request.size());
    if(body==Read::Failed)return TransportResult::IoFailure;
    if(body!=Read::Complete)return TransportResult::InvalidFrame;
    std::string reply,error;bool success=false;
    try {success=handler(request,&reply,&error);} catch(...) {error="engine_failed";}
    if(!success)reply=error_reply(error);
    else if(reply.empty())reply=error_reply("engine_failed");
    else if(reply.size()>limit)reply=error_reply("decision_result_limit");
    const auto bytes=static_cast<uint32_t>(reply.size());
    const unsigned char prefix[]={static_cast<unsigned char>(bytes>>24),static_cast<unsigned char>(bytes>>16),
        static_cast<unsigned char>(bytes>>8),static_cast<unsigned char>(bytes)};
    if(!write_exact(output,prefix,sizeof(prefix))||!write_exact(output,reply.data(),reply.size()))
      return TransportResult::IoFailure;
  }
}
}

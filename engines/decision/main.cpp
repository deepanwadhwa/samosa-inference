// Private persistent worker. The owning provider verifies/snapshots assets and
// applies process admission, deadlines and cancellation before launching it.
#include "native_text.h"
#include "native_image.h"
#include "image_pixels.h"
#include "request.h"
#include "transport.h"
#include <csignal>
#include <charconv>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
int main(int argc,char** argv) {
  try {
    std::string model,tokenizer,image;
    int width=0,height=0;
    for(int i=1;i<argc;++i) {
      const std::string option=argv[i];
      if(option=="--model"&&model.empty()&&i+1<argc)model=argv[++i];
      else if(option=="--tokenizer"&&tokenizer.empty()&&i+1<argc)tokenizer=argv[++i];
      else if(option=="--image-rgba"&&image.empty()&&i+1<argc)image=argv[++i];
      else if((option=="--width"||option=="--height")&&i+1<argc) {
        int& value=option=="--width"?width:height;
        if(value)return 2;
        const std::string number=argv[++i];
        auto parsed=std::from_chars(number.data(),number.data()+number.size(),value);
        if(parsed.ec!=std::errc{}||parsed.ptr!=number.data()+number.size()||value<1||value>512)return 2;
      }
      else return 2;
    }
    if(model.empty()||tokenizer.empty()||model[0]!='/'||tokenizer[0]!='/')return 2;
    if(image.empty()?(width||height):(!width||!height||image[0]!='/'))return 2;
    std::signal(SIGPIPE,SIG_IGN);
    std::ifstream input(tokenizer,std::ios::binary|std::ios::ate);
    if(!input.good()||input.tellg()<=0||input.tellg()>16777216)return 3;
    input.seekg(0);std::ostringstream bytes;bytes<<input.rdbuf();
    if(!input.good())return 3;
    std::string error;
    if(!image.empty()) {
      int fd=open(image.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK);
      struct stat info={};
      size_t count=static_cast<size_t>(width)*height*4;
      if(fd<0)return 3;
      if(fstat(fd,&info)||!S_ISREG(info.st_mode)||info.st_uid!=geteuid()||
          (info.st_mode&022)||info.st_size!=static_cast<off_t>(count)){close(fd);return 3;}
      std::vector<uint8_t> rgba(count);size_t at=0;
      while(at<count) {
        ssize_t n=read(fd,rgba.data()+at,count-at);
        if(n<0&&errno==EINTR)continue;
        if(n<=0){close(fd);return 3;}
        at+=static_cast<size_t>(n);
      }
      close(fd);std::vector<float> pixels;
      if(!samosa::decision::image_pixels(rgba,width,height,4,&pixels,&error))return 3;
      samosa::decision::NativeImage native;
      if(!native.load(model,bytes.str(),&error)){std::cerr<<"decision image load failed\n";return 3;}
      const auto backend=samosa::decision::image_backend(native,pixels);
      const auto status=samosa::decision::serve_requests(STDIN_FILENO,STDOUT_FILENO,
          [&](const auto& request,auto* reply,auto* why) {
            return samosa::decision::execute_request(backend,request,reply,why);
          });
      return status==samosa::decision::TransportResult::Eof?0:4;
    }
    samosa::decision::NativeText native;
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

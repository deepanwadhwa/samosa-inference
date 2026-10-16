#include "decision/native_image.h"
#include "decision/image_pixels.h"
#include "decision/tokenizer/request.h"
#include "json.h"
#include "sha256.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#define CHECK(value) do { if (!(value)) throw std::runtime_error(#value); } while (0)
#include "decision_reference_fixture.h"
static std::string read(const std::filesystem::path& path, size_t bound) {
  std::ifstream input(path,std::ios::binary|std::ios::ate);
  CHECK(input.good()&&input.tellg()>0&&static_cast<size_t>(input.tellg())<=bound);
  input.seekg(0);std::ostringstream body;body<<input.rdbuf();return body.str();
}
static void verify(const std::filesystem::path& path,const char* pin) {
  std::ifstream input(path,std::ios::binary);CHECK(input.good());
  Sha256 sha;sha256_init(&sha);char block[65536];
  while(input){input.read(block,sizeof(block));sha256_update(&sha,block,static_cast<size_t>(input.gcount()));}
  CHECK(input.eof());unsigned char raw[32];char hex[65];sha256_final(&sha,raw);sha256_hex(raw,hex);
  CHECK(std::string(hex)==pin);
}
static double maximum=0;
static void compare(jval* actual,jval* expected) {
  CHECK(actual&&expected&&actual->t==expected->t);
  if(expected->t==J_NUM){double delta=std::abs(actual->num-expected->num);CHECK(std::isfinite(actual->num)&&delta<=0.005);maximum=std::max(maximum,delta);}
  else if(expected->t==J_STR) CHECK(std::string(actual->str)==expected->str);
  else if(expected->t==J_ARR||expected->t==J_OBJ) {
    CHECK(actual->len==expected->len);
    for(int index=0;index<expected->len;++index) {
      if(expected->t==J_OBJ) CHECK(std::string(actual->keys[index])==expected->keys[index]);
      compare(actual->kids[index],expected->kids[index]);
    }
  }
}
int main(int argc,char** argv) {
  try {
    CHECK(argc==7); // MODEL TOKENIZER REPORT MODEL_SHA TOKENIZER_SHA REPORT_SHA
    verify(argv[1],argv[4]);verify(argv[2],argv[5]);verify(argv[3],argv[6]);
    auto tokenizer=read(argv[2],16777216),body=read(argv[3],2097152);
    char* arena=nullptr;jval* report=json_parse(body.c_str(),&arena);
    CHECK(report);
    struct Cleanup{jval* value;char* arena;~Cleanup(){json_free(value);free(arena);}} cleanup{report,arena};
    samosa::decision::NativeImage model;
    std::string error;
    CHECK(model.load(argv[1],tokenizer,&error));
    CHECK(!model.load(argv[1],"{}",&error));
    CHECK(!model.load("/missing/samosa-image-model.onnx",tokenizer,&error));
    auto* cases=json_get(report,"cases");CHECK(cases&&cases->t==J_ARR&&cases->len==11);
    for(int index=0;index<cases->len;++index) {
      auto* item=cases->kids[index];auto* state=json_get(item,"state");
      auto* file=json_get(state,"file");auto* pin=json_get(state,"sha256");
      auto* width=json_get(state,"width");auto* height=json_get(state,"height");
      CHECK(file&&file->t==J_STR&&pin&&pin->t==J_STR&&width&&height);
      std::filesystem::path relative(file->str);CHECK(relative.filename()==relative&&!relative.is_absolute());
      auto path=std::filesystem::path(argv[3]).parent_path()/relative;verify(path,pin->str);
      auto raw=read(path,512*512*3);std::vector<uint8_t> rgb(raw.begin(),raw.end());
      std::vector<float> pixels;
      CHECK(samosa::decision::image_pixels(rgb,width->num,height->num,3,&pixels,&error));
      auto backend=samosa::decision::image_backend(model,pixels);
      auto* method=json_get(item,"method");auto* arguments=json_get(item,"arguments");
      CHECK(method&&method->t==J_STR&&arguments&&arguments->t==J_OBJ);
      std::string request="{\"method\":"+fixture_json(method)+",\"state\":\"frame-identity\"";
      for(int field=0;field<arguments->len;++field) {
        jval key={};key.t=J_STR;key.str=arguments->keys[field];
        request+=','+fixture_json(&key)+':'+fixture_json(arguments->kids[field]);
      }
      request+='}';std::string result="sentinel";
      if(!samosa::decision::execute_request(backend,request,&result,&error)) throw std::runtime_error(error);
      void* admitted=nullptr;
      CHECK(!sm_decision_request_load(reinterpret_cast<const uint8_t*>(request.data()),request.size(),&admitted));
      CHECK(!sm_decision_result_validate_profile(admitted,reinterpret_cast<const uint8_t*>(result.data()),result.size(),1));
      CHECK(sm_decision_result_validate_profile(admitted,reinterpret_cast<const uint8_t*>(result.data()),result.size(),99));
      if(std::string(method->str)=="relation")
        CHECK(sm_decision_result_validate(admitted,reinterpret_cast<const uint8_t*>(result.data()),result.size()));
      sm_decision_request_free(admitted);
      char* result_arena=nullptr;jval* actual=json_parse(result.c_str(),&result_arena);
      compare(actual,json_get(item,"result"));json_free(actual);free(result_arena);
      auto retained=result;
      CHECK(!samosa::decision::execute_request(backend,"{}",&result,&error)&&result==retained);
      std::cout<<json_get(item,"name")->str<<" typed result matches"<<std::endl;
    }
    std::cout<<"PASS native typed image: 11 cases; max_result_error="<<maximum<<'\n';return 0;
  } catch(const std::exception& exception){std::cerr<<"FAIL native typed image: "<<exception.what()<<'\n';return 1;}
}

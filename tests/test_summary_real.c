#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include "summary_fixture.h"
int main(int argc, char **argv) {
  CHECK((argc == 4 || (argc == 5 && !strcmp(argv[4],"--registry"))) && argv[1][0] == '/' && argv[2][0] == '/' &&
        argv[3][0] == '/');
  char root[] = "/tmp/samosa-summary-real-qa-XXXXXX";
  CHECK(mkdtemp(root));
  char state[512];
  snprintf(state, sizeof(state), "%s/state", root);
  FixtureConfig c = {.cli = argv[1],
                     .engine = argv[2],
                     .model = argv[3],
                     .state = state,
                     .owner = "actual-summary"};
  file_sha(c.engine, c.engine_sha);
  file_sha(c.model, c.model_sha);
  CHECK(!strcmp(
      c.model_sha,
      "30c73e0b2ebd4f65538d87f5157aa7193261e8ff362e441df86c6941f3e9265c"));
  if(argc==5){
    char path[512];snprintf(path,sizeof(path),"%s/registry.json",root);
    char *config=NULL;size_t bytes;FILE *out=open_memstream(&config,&bytes);CHECK(out);
    fputs("{\"contract_version\":1,\"owner\":\"actual-summary\",\"providers\":[{\"kind\":\"summarization\",\"provider_id\":\"native-t5-summary-v1\",\"state\":",out);
    json_quote(out,state,strlen(state));fputs(",\"engine\":",out);json_quote(out,c.engine,strlen(c.engine));
    fputs(",\"model\":",out);json_quote(out,c.model,strlen(c.model));
    fprintf(out,",\"engine_sha256\":\"%s\",\"model_sha256\":\"%s\"},{\"kind\":\"fake_text\",\"provider_id\":\"contract-text\",\"state\":\"%s/text-state\"}]}",c.engine_sha,c.model_sha,root);
    CHECK(!fclose(out));file_write(path,config);free(config);CHECK(!setenv("SAMOSA_TEST_REGISTRY_CONFIG",path,1));
  }else CHECK(!getenv("SAMOSA_TEST_REGISTRY_CONFIG"));
  const char *input = "The city council approved a new public library. "
                      "Construction begins in March.";
  char *req = request(input, "actual-summary", 60000),
       *body = invoke(&c, req, NULL);
  Json j;
  parse(body, &j);
  char *text = string_field(body, "text");
  CHECK(*text && strstr(text, "library") && strstr(text, "March"));
  CHECK(!strstr(text, "Fake"));
  free(text);
  char digest[65];
  hash_bytes(input, strlen(input), digest);
  char *actual = string_field(body, "input_sha256");
  CHECK(!strcmp(actual, digest));
  free(actual);
  int provenance = json_get(&j, 0, "provenance");
  CHECK(provenance >= 0);
  actual = json_string(&j, json_get(&j, provenance, "engine_sha256"));
  CHECK(actual && !strcmp(actual, c.engine_sha));
  free(actual);
  actual = json_string(&j, json_get(&j, provenance, "model_sha256"));
  CHECK(actual && !strcmp(actual, c.model_sha));
  free(actual);
  int consumed = json_get(&j, provenance, "processed_input_tokens");
  CHECK(consumed >= 0 && j.tokens[consumed].type == JT_NULL);
  int coverage = json_get(&j, 0, "coverage");
  actual = json_string(&j, json_get(&j, coverage, "status"));
  CHECK(actual && !strcmp(actual, "partial"));
  free(actual);
  char *replay = invoke(&c, req, NULL);
  CHECK(!strcmp(body, replay));
  free(replay);
  free(body);
  free(req);
size_t long_length=0;char *long_input=calloc(60001,1);CHECK(long_input);
for(size_t i=0;i<180;i++){
  int added=snprintf(long_input+long_length,60001-long_length,"Report %zu: The city council approved a new public library and a public hospital. Construction begins in March. The hospital will offer emergency care and the library will provide free computers. Residents voted to fund the project through a municipal grant. The mayor announced that the buildings will open next year.\n",i);
  CHECK(added>0&&(size_t)added<60001-long_length);long_length+=(size_t)added;
}
  req = request(long_input, NULL, 60000);
  body = invoke(&c, req, "invalid_provider_response");
  free(body);
  free(req);
  req = request_opening(long_input, "actual-opening", 60000);
  body = invoke(&c, req, NULL);
  parse(body, &j);
  text = string_field(body, "text");
  CHECK(strchr(text, '\n') && strstr(text, "library"));
  free(text);
  hash_bytes(long_input, long_length, digest);
  actual = string_field(body, "input_sha256");
  CHECK(!strcmp(actual, digest));
  free(actual);
  size_t supplied = 0;
  CHECK(json_uint(&j, json_get(&j, 0, "input_bytes"), &supplied) &&
        supplied == long_length);
  provenance = json_get(&j, 0, "provenance");
  actual = json_string(&j, json_get(&j, provenance, "recipe"));
  CHECK(actual &&
        !strcmp(actual, "samosa-native-summary-opening-map-reduce-v1"));
  free(actual);
  actual = json_string(&j, json_get(&j, provenance, "input_partitioning"));
  CHECK(actual && !strcmp(actual, "native_token_chunks"));
  free(actual);
  size_t mapped = 0, rounds = 0;
  CHECK(json_uint(&j, json_get(&j, provenance, "map_output_bytes"), &mapped) &&
        mapped > 1800);
  CHECK(json_uint(&j, json_get(&j, provenance, "reduction_rounds"), &rounds) &&
        rounds > 0);
  consumed = json_get(&j, provenance, "processed_input_tokens");
  CHECK(consumed >= 0 && j.tokens[consumed].type == JT_NULL);
  replay = invoke(&c, req, NULL);
  CHECK(!strcmp(body, replay));
  free(replay);
  free(body);
  free(req);
  free(long_input);
  cleanup(root);

  printf("PASS actual native T5 public summary%s: %u checks; input/model/engine "
         "provenance and replay\n",
         argc==5?" through registry":"",checks);
  return 0;
}

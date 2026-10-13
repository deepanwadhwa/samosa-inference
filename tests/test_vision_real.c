#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include "vision_fixture.h"
/* This gate invokes the existing native engine with actual checkpoint files.
 * A rendered monolith PDF fixture supplies the established heading expectation. */
int main(int argc, char **argv) {
    CHECK(argc == 5);
    alarm(180);
    char root[] = "/tmp/samosa-vision-real-XXXXXX";
    CHECK(mkdtemp(root));
    char state[512], path[4096];
    snprintf(state, sizeof(state), "%s/state", root);
    VisionFixture cfg = {.base = {.cli = argv[1],
                                  .engine = argv[2],
                                  .model = argv[3],
                                  .state = state,
                                  .owner = "vision-real-owner"}};
    file_sha(cfg.base.engine, cfg.base.engine_sha);
    snprintf(path, sizeof(path), "%s/config.json", cfg.base.model);
    file_sha(path, cfg.config);
    snprintf(path, sizeof(path), "%s/tokenizer.json", cfg.base.model);
    file_sha(path, cfg.tokenizer);
    snprintf(path, sizeof(path), "%s/model.safetensors", cfg.base.model);
    file_sha(path, cfg.weights);
    if(getenv("SAMOSA_TEST_USE_REGISTRY")){
        char material[256],extra[320];int n=snprintf(material,sizeof(material),"visionpsy-model-v1:%s:%s:%s",cfg.config,cfg.tokenizer,cfg.weights);CHECK(n>0&&(size_t)n<sizeof(material));
        hash_bytes(material,(size_t)n,cfg.base.model_sha);
        snprintf(extra,sizeof(extra),",\"config_sha256\":\"%s\",\"tokenizer_sha256\":\"%s\",\"weights_sha256\":\"%s\"",cfg.config,cfg.tokenizer,cfg.weights);
        actual_registry(&cfg.base,root,"visionpsy-native-v1","image_analysis",extra);
    }else CHECK(!getenv("SAMOSA_TEST_REGISTRY_CONFIG"));
    FILE *f = fopen(argv[4], "rb");
    CHECK(f);
    size_t width = 0, height = 0;
    CHECK(fscanf(f, "P6\n%zu %zu\n255", &width, &height) == 2 && width && height && width <= 4096 &&
          height <= 4096);
    CHECK(fgetc(f) == '\n');
    size_t bytes = width * height * 3;
    unsigned char *rgb = malloc(bytes);
    CHECK(rgb && fread(rgb, 1, bytes, f) == bytes && fgetc(f) == EOF);
    CHECK(!fclose(f));
    size_t w = width, h = height;
    if (w > 512 || h > 512) {
        if (w >= h) {
            h = height * 512 / width;
            w = 512;
        } else {
            w = width * 512 / height;
            h = 512;
        }
    }
    CHECK(w && h);
    unsigned char *rgba = malloc(w * h * 4);
    CHECK(rgba);
    for (size_t y = 0; y < h; y++)
        for (size_t x = 0; x < w; x++) {
            size_t source = ((y * height / h) * width + x * width / w) * 3,
                   target = (y * w + x) * 4;
            memcpy(rgba + target, rgb + source, 3);
            rgba[target + 3] = 255;
        }
    free(rgb);
    char input_sha[65];
    hash_bytes(rgba, w * h * 4, input_sha);
    char *pixels = encode(rgba, w * h * 4);
    free(rgba);
    char *req = request(pixels, w, h, "Read the large heading on this page. Answer briefly.",
                        "real-heading", 60000);
    free(pixels);
    char *body = vinvoke(&cfg, req, NULL), *text = string_field(body, "text");
    for (char *p = text; *p; p++)
        *p = (char)tolower((unsigned char)*p);
    if (!strstr(text, "hello"))
        fprintf(stderr, "actual observation: %s\n", text);
    CHECK(strstr(text, "hello") && strstr(body, input_sha) && strstr(body, cfg.base.engine_sha) &&
          strstr(body, "visual_analysis_uncertain"));
    char *replay = vinvoke(&cfg, req, NULL);
    CHECK(!strcmp(body, replay));
    free(replay);
    free(req);
    printf("PASS actual public VisionPsy heading gate: %u checks; %zux%zu prepared pixels, pinned "
           "native engine/checkpoint, derived observation and durable replay; text=%s; bounded "
           "fixture recognition only\n",
           checks, w, h, text);
    free(text);
    free(body);
    cleanup(root);
    return 0;
}

/* Loader-only QA dependency; never model execution or a shipped ORT runtime. */
const void *OrtGetApiBase(void) {static const int fixture=1;return &fixture;}

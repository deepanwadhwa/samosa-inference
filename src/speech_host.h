#ifndef SAMOSA_SPEECH_HOST_H
#define SAMOSA_SPEECH_HOST_H
#include "activation.h"
#include "speech.h"
#include "speech_worker.h"
#include "speech_package.h"
typedef struct {
    Activation activation;
    SpeechResident transport;
    InferenceConfig inference;
    SpeechPackageReceipt package;
    struct stat worker_before,manifest_before;
    char state[4096],manifest[4096],worker_pin[65];
    size_t worker_size,manifest_size;
    int package_fd,initialized,attempted,qualified,activating;
} SpeechHost;
const char *speech_host_init(SpeechHost *,const InferenceConfig *,const char *,size_t,size_t);
const char *speech_host_start(SpeechHost *,const InferenceConfig *);
const char *speech_host_status(void *);
const char *speech_host_stop(SpeechHost *);
const char *speech_host_run(void *,const InferenceConfig *,const char *,size_t,int,
    SpeechPcmSink,void *,SpeechRuntimeResult *);
#endif

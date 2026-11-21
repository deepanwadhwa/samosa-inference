#define _POSIX_C_SOURCE 200809L
#include "speech_session.h"
#include "speech_package_loader.h"
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
struct SpeechSession {
    int root;
    char *path, *pin;
    SpeechPackageReceipt package;
    SpeechRuntime *runtime;
    const char *caro, *stuart;
};
void speech_session_close(SpeechSession *s) {
    if(!s)return;
    speech_runtime_close(s->runtime);
    if(s->root>=0)close(s->root);
    free(s->path);free(s->pin);free(s);
}
const char *speech_session_finish(SpeechSession *s) {
    if(!s)return "invalid_configuration";
    speech_runtime_close(s->runtime);s->runtime=NULL;
    const char *e=speech_package_verify(s->root,s->path,s->pin,NULL,NULL,&s->package,1);
    speech_session_close(s);return e;
}
static const char *voice_check(const SpeechSession *s,const char *voice) {
    return s->package.pocket&&voice&&strcmp(voice,"caro")&&strcmp(voice,"stuart")?"invalid_speech_voice":NULL;
}
const char *speech_session_open(int root,const char *path,const char *pin,
    const char *voice,const char *(*control)(void *),void *context,SpeechSession **out) {
    if(!out)return "invalid_configuration";
    *out=NULL;
    if(!path||!pin)return "invalid_configuration";
    SpeechSession *s=calloc(1,sizeof(*s));if(!s)return "speech_resource_limit";
    s->root=fcntl(root,F_DUPFD_CLOEXEC,6);s->path=strdup(path);s->pin=strdup(pin);
    const char *e=s->root<0||!s->path||!s->pin?"speech_resource_limit":NULL;
    if(!e)e=speech_package_verify(s->root,s->path,s->pin,control,context,&s->package,0);
    LibraryRuntimePolicy policy;
    if(!e)e=library_runtime_host_policy(&policy);
    if(!e)e=speech_package_loader_verify(s->root,s->path,s->pin,&policy,control,context,&s->package);
    SpeechRuntimeOptions options;
    if(!e)e=speech_package_options(&s->package,&options,&s->caro,&s->stuart);
    if(!e)e=voice_check(s,voice);
    if(!e)e=speech_runtime_open(&options,&s->runtime);
    if(e)speech_session_close(s);else *out=s;
    return e;
}
const char *speech_session_generate(SpeechSession *s,const char *text,const char *voice,
    SpeechPcmSink sink,void *sink_context,const char *(*control)(void *),void *context,
    SpeechRuntimeResult *result) {
    if(!result)return "invalid_configuration";
    memset(result,0,sizeof(*result));
    if(!s||!s->runtime)return "invalid_configuration";
    const char *e=voice_check(s,voice);
    if(!e)e=speech_package_verify(s->root,s->path,s->pin,control,context,&s->package,1);
    if(!e)e=speech_runtime_generate(s->runtime,text,voice,
        s->package.pocket?(voice&&!strcmp(voice,"stuart")?s->stuart:s->caro):NULL,
        sink,sink_context,result);
    if(!e)e=speech_package_verify(s->root,s->path,s->pin,control,context,&s->package,1);
    if(e)memset(result,0,sizeof(*result));
    return e;
}

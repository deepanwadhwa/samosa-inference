#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include <samosa_inference/visual_lifecycle.h>
#include "activation.h"
#include "artifact.h"
#include "engine_runtime.h"
#include "input_identity.h"
#include "json.h"
#include "sha256.h"
#include "specialist_supervisor_internal.h"
#include "temporal_package.h"
#include <fcntl.h>
#include <limits.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

enum { VISION_FILES = 6, PROBE_LIMIT = 16 * 1024 * 1024 };
static const char *vision_files[VISION_FILES] = {
    "model.safetensors", "tokenizer.json", "preprocessor_config.json",
    "processor_config.json", "tokenizer_config.json", "chat_template.jinja"
};

struct SamosaVisualLifecycle {
    SamosaVisualLifecycleConfig config;
    Activation activation;
    SamosaMmSupervisor *supervisor;
    SamosaMmSession *session;
    SamosaMmControl control;
    const SamosaMmTrace *trace;
    int directory, kernel, probe, attempted, qualified;
    struct stat engine_receipt, anchor_receipt, kernel_receipt, probe_receipt;
    struct stat directory_receipt, model_receipts[VISION_FILES];
    TemporalPackageReceipt temporal;
    char self[PATH_MAX], launch_error[SAMOSA_MM_ERROR_MAX];
};

static int64_t now_ms(void) {
    struct timespec ts;
    return clock_gettime(CLOCK_MONOTONIC,&ts)?-1:(int64_t)ts.tv_sec*1000+ts.tv_nsec/1000000;
}

static const char *interrupted(void *context) {
    SamosaVisualLifecycle *h=context;
    const char *error=h->control.check?h->control.check(h->control.context):NULL;
    if (!error && h->control.deadline_ms>0 && now_ms()>=h->control.deadline_ms)
        error="multimodal_deadline_exceeded";
    return error;
}

static int private_input(const struct stat *st) {
    return st->st_uid==geteuid() && !(st->st_mode&022);
}

static int same_input(const struct stat *before, const struct stat *now) {
    return inference_same_input(before,now) &&
        before->st_uid==now->st_uid && before->st_nlink==now->st_nlink;
}

static int child_path(const char *path, const char *parent, size_t length, const char *name) {
    return !strncmp(path,parent,length) && path[length]=='/' && !strcmp(path+length+1,name);
}

static int valid_pin(const SamosaVisualInput *input, uint64_t ceiling) {
    return input->path && input->path[0]=='/' && input->sha256 && strlen(input->sha256)==64 &&
        strspn(input->sha256,"0123456789abcdef")==64 && input->size && input->size<=ceiling;
}

static const char *verify_file(SamosaVisualLifecycle *h, int directory, const char *name,
    const SamosaVisualInput *input, uint64_t ceiling, int *retained, struct stat *receipt) {
    int fd=openat(directory,name,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
    if (fd<0) return "artifact_unavailable";
    const char *error=NULL;
    struct stat now;
    if (fstat(fd,receipt) || !S_ISREG(receipt->st_mode) || !private_input(receipt) || receipt->st_nlink!=1)
        error="unsupported_artifact";
    if (!error) error=artifact_verify_controlled(fd,input->size,ceiling,input->sha256,interrupted,h);
    if (!error && (fstat(fd,&now) || !same_input(receipt,&now) ||
        fstatat(directory,name,&now,AT_SYMLINK_NOFOLLOW) || !same_input(receipt,&now)))
        error="artifact_changed";
    if (!error && retained) *retained=fd;
    else close(fd);
    return error;
}

static const char *owned(void *context) {
    SamosaVisualLifecycle *h=context;
    const SamosaVisualLifecycleConfig *c=&h->config;
    const char *error=activation_owned_status(&h->activation,c->state,c->engine.path,c->anchor.path,
        &h->engine_receipt,&h->anchor_receipt);
    if (error) return error;
    if (!activation_input_unchanged(h->kernel,c->kernel.path,&h->kernel_receipt) ||
        !activation_input_unchanged(h->probe,c->probe.path,&h->probe_receipt))
        return "provider_artifact_changed";
    if (c->profile==SAMOSA_VISUAL_MOLMO)
        return temporal_package_unchanged(h->directory,c->model_directory,&h->temporal)?
            NULL:"provider_artifact_changed";
    struct stat held,named;
    if (fstat(h->directory,&held) || lstat(c->model_directory,&named) ||
        !same_input(&h->directory_receipt,&held) || !same_input(&held,&named))
        return "provider_artifact_changed";
    for (size_t i=0;i<VISION_FILES;++i)
        if (fstatat(h->directory,vision_files[i],&named,AT_SYMLINK_NOFOLLOW) ||
            !same_input(&h->model_receipts[i],&named)) return "provider_artifact_changed";
    return NULL;
}

static const char *verify_bundle(void *context, int engine, int anchor) {
    SamosaVisualLifecycle *h=context;
    const SamosaVisualLifecycleConfig *c=&h->config;
    const char *error=engine_system_runtime(engine);
    if (error) return error;
    if (fstat(engine,&h->engine_receipt) || fstat(anchor,&h->anchor_receipt))
        return "artifact_unavailable";
    error=verify_file(h,AT_FDCWD,c->kernel.path,&c->kernel,UINT64_C(536870912),&h->kernel,&h->kernel_receipt);
    if (!error) error=verify_file(h,AT_FDCWD,c->probe.path,&c->probe,PROBE_LIMIT,&h->probe,&h->probe_receipt);
    if (error) return error;
    h->directory=open(c->model_directory,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if (h->directory<0) return "artifact_unavailable";
    if (fstat(h->directory,&h->directory_receipt) || !private_input(&h->directory_receipt))
        return "unsupported_artifact";
    if (c->profile==SAMOSA_VISUAL_MOLMO)
        return temporal_package_verify(h->directory,c->model_directory,c->anchor.sha256,interrupted,h,&h->temporal,0);
    for (size_t i=0;i<VISION_FILES;++i) {
        error=verify_file(h,h->directory,vision_files[i],&c->model_files[i],UINT64_C(8589934592),
            NULL,&h->model_receipts[i]);
        if (error) return error;
    }
    return NULL;
}

static const char *launch(void *context, const char *self, const char *argv, double lifetime, size_t limit,
    int lease, int anchor, EngineProcess *process) {
    (void)self;(void)argv;(void)lifetime;(void)limit;(void)anchor;
    SamosaVisualLifecycle *h=context;
    const SamosaVisualLifecycleConfig *c=&h->config;
    SamosaMmProvider provider=mm_visual_profile(c->profile==SAMOSA_VISUAL_MOLMO,c->engine.path,c->model_directory);
    if (mm_session_start_admitted(h->supervisor,&provider,&h->control,h->session,process,lease,
        h->launch_error,sizeof(h->launch_error))) return NULL;
    if (process->terminal && !strcmp(process->reason,"engine_guardian_failed")) return "engine_guardian_failed";
    return h->launch_error;
}

static const char *qualify(void *context) {
    SamosaVisualLifecycle *h=context;
    const SamosaVisualLifecycleConfig *c=&h->config;
    const char *error=owned(h);
    if (error) return error;
    unsigned char *image=malloc((size_t)c->probe.size);
    if (!image) return "resource_limit";
    size_t bytes=0;
    while (!error && bytes<c->probe.size) {
        error=interrupted(h);
        if (error) break;
        ssize_t got=pread(h->probe,image+bytes,(size_t)c->probe.size-bytes,(off_t)bytes);
        if (got<0 && errno==EINTR) continue;
        if (got<=0) error="qualification_image_unavailable";
        else bytes+=(size_t)got;
    }
    char file[PATH_MAX];
    if (!error) error=owned(h);
    if (!error && !mm_session_probe_file(h->session,image,bytes,file,sizeof(file)))
        error="engine_io_failed";
    free(image);
    if (error) return error;
    SamosaMmControl bounded=h->control;
    int64_t now=now_ms();
    if (now<0) return "clock_failed";
    const int64_t deadline=now+60000;
    if (!bounded.deadline_ms || bounded.deadline_ms>deadline) bounded.deadline_ms=deadline;
    mm_session_probe_control(h->session,&bounded);
    char text[4096]={0}, detail[SAMOSA_MM_ERROR_MAX]={0};
    int generated=0, ok;
    if (c->profile==SAMOSA_VISUAL_MOLMO)
        ok=samosa_mm_molmo2_analyze_request(h->session,h->trace,file,NULL,0,"image",
            c->probe_prompt,32,0,0,1,text,sizeof(text),NULL,&generated,NULL,NULL,NULL,detail,sizeof(detail));
    else
        ok=samosa_mm_visionpsy_inspect(h->session,h->trace,file,c->probe_prompt,1536,32,
            text,sizeof(text),NULL,&generated,detail,sizeof(detail));
    mm_session_probe_control(h->session,NULL);
    if (!ok || generated<=0) return "provider_qualification_failed";
    size_t expected=strlen(c->probe_expected);
    for (const char *p=text;*p;++p)
        if (!strncasecmp(p,c->probe_expected,expected)) return owned(h);
    return "provider_qualification_failed";
}

const char *samosa_visual_lifecycle_create(const SamosaVisualLifecycleConfig *c, SamosaVisualLifecycle **out) {
    if (!out) return "invalid_visual_host_configuration";
    *out=NULL;
    if (!c || (c->profile!=SAMOSA_VISUAL_VISIONPSY&&c->profile!=SAMOSA_VISUAL_MOLMO) ||
        !c->state || c->state[0]!='/' || !c->model_directory || c->model_directory[0]!='/' ||
        !valid_pin(&c->engine,67108864) || !valid_pin(&c->kernel,536870912) ||
        !valid_pin(&c->anchor,1048576) || !valid_pin(&c->probe,PROBE_LIMIT) ||
        !c->probe_prompt || !*c->probe_prompt || strlen(c->probe_prompt)>1024 ||
        !c->probe_expected || !*c->probe_expected || strlen(c->probe_expected)>128)
        return "invalid_visual_host_configuration";
    size_t model_length=strlen(c->model_directory);
    const char *parent=strrchr(c->engine.path,'/');
    int molmo=c->profile==SAMOSA_VISUAL_MOLMO;
    if (!parent || !child_path(c->kernel.path,c->engine.path,(size_t)(parent-c->engine.path),"mlx.metallib") ||
        !child_path(c->anchor.path,c->model_directory,model_length,molmo?"manifest.json":"config.json") ||
        c->model_file_count!=(molmo?0:VISION_FILES) || (!molmo&&!c->model_files))
        return "invalid_visual_host_configuration";
    for (size_t i=0;i<c->model_file_count;++i)
        if (!valid_pin(&c->model_files[i],UINT64_C(8589934592)) ||
            !child_path(c->model_files[i].path,c->model_directory,model_length,vision_files[i]))
            return "invalid_visual_host_configuration";
    SamosaVisualLifecycle *h=calloc(1,sizeof(*h));
    if (!h) return "resource_limit";
    if (!engine_process_executable(h->self)) { free(h);return "engine_unavailable"; }
    h->config=*c;
    h->directory=h->kernel=h->probe=-1;
    *out=h;
    return NULL;
}

const char *samosa_visual_lifecycle_status(const SamosaVisualLifecycle *h) {
    if (!h || !h->attempted) return "provider_unavailable";
    if (atomic_load(&h->activation.cleanup_failed)) return "engine_guardian_failed";
    if (!h->qualified || !h->session || !h->session->active) return "provider_unavailable";
    return owned((void *)h);
}

const char *samosa_visual_lifecycle_stop(SamosaVisualLifecycle *h) {
    if (!h || !h->attempted) return NULL;
    h->qualified=0;
    const char *error=samosa_mm_session_close_owned(h->session);
    if (error) {
        if (!strcmp(error,"engine_guardian_failed")) activation_cleanup_failed(&h->activation);
        return error;
    }
    error=activation_stop(&h->activation);
    if (error && (!strcmp(error,"engine_shutdown_pending") || !strcmp(error,"engine_guardian_failed"))) return error;
    if (h->directory>=0) close(h->directory);
    if (h->probe>=0) close(h->probe);
    if (h->kernel>=0) close(h->kernel);
    h->directory=h->probe=h->kernel=-1;
    h->session=NULL;
    h->attempted=0;
    return NULL;
}

const char *samosa_visual_lifecycle_destroy(SamosaVisualLifecycle *h) {
    const char *error=samosa_visual_lifecycle_stop(h);
    if (!error) free(h);
    return error;
}

const char *samosa_visual_lifecycle_start(SamosaVisualLifecycle *h,SamosaMmSupervisor *supervisor,
    SamosaMmSession *session,const SamosaMmControl *control,const SamosaMmTrace *trace) {
    if (!h || !supervisor || !session) return "invalid_visual_host_configuration";
    if (h->attempted) return h->session==session?samosa_visual_lifecycle_status(h):"provider_busy";
    if (session->supervisor || session->active || session->guardian || session->lifecycle.close)
        return "multimodal_session_owned";
    h->supervisor=supervisor;
    h->session=session;
    h->control=control?*control:(SamosaMmControl){0};
    h->trace=trace;
    const SamosaVisualLifecycleConfig *c=&h->config;
    char *bundle=NULL;
    size_t bytes=0;
    FILE *out=open_memstream(&bundle,&bytes);
    if (!out) return "resource_limit";
    fprintf(out,"{\"profile\":\"%s\",\"metal_sha256\":\"%s\",\"probe_sha256\":\"%s\",\"probe_prompt\":",
        c->profile==SAMOSA_VISUAL_MOLMO?"default-molmo2":"default-visionpsy",c->kernel.sha256,c->probe.sha256);
    json_quote(out,c->probe_prompt,strlen(c->probe_prompt));
    fputs(",\"expected_text\":",out);json_quote(out,c->probe_expected,strlen(c->probe_expected));
    fputs(",\"model_inputs\":[",out);
    for (size_t i=0;i<c->model_file_count;++i)
        fprintf(out,"%s{\"name\":\"%s\",\"sha256\":\"%s\",\"size\":%llu}",i?",":"",vision_files[i],
            c->model_files[i].sha256,(unsigned long long)c->model_files[i].size);
    fputs("]}",out);
    if (fclose(out)) { free(bundle);return "resource_limit"; }
    char identity[65];
    hash_bytes(bundle,bytes,identity);
    ActivationConfig activation={.state=c->state,.self=h->self,.engine=c->engine.path,.model=c->anchor.path,
        .engine_sha=c->engine.sha256,.model_sha=c->anchor.sha256,.engine_size=c->engine.size,
        .model_size=c->anchor.size,.byte_ceiling=67108864,.advance_current=1,.readable_inputs=1,
        .argv_template="[\"@ENGINE@\",\"@MODEL@\"]",.model_argument=c->model_directory,
        .verify_runtime=verify_bundle,.runtime_context=h,.bundle_identity=identity,.bundle_config=bundle,
        .launch=launch,.launch_context=h,.qualify=qualify,.qualification=h};
    h->attempted=1;
    const char *error=activation_start(&activation,&h->activation);
    free(bundle);
    h->control=(SamosaMmControl){0};
    h->trace=NULL;
    if (error) {
        const char *cleanup=samosa_visual_lifecycle_stop(h);
        return cleanup?cleanup:error;
    }
    h->qualified=1;
    mm_session_set_verifier(session,owned,h);
    mm_session_publish(session);
    return NULL;
}

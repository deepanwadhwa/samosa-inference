/* Trusted test host for the production guardian and reused summary transport.
 * Pins/activation/public readiness are separate integration gates. */
#ifndef SAMOSA_TEST_SUMMARY_OWNED_FIXTURE_H
#define SAMOSA_TEST_SUMMARY_OWNED_FIXTURE_H
#include <samosa_inference/summary_process.h>
#include "engine_process.h"
#include "json.h"
#include <fcntl.h>
#include <poll.h>
#include <stdatomic.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
typedef struct {
    EngineProcess process;
    const char *self,*mode;
    char lease[1024],marker[1024],log[1024],workspace[1024];
    int write_fd,read_fd;
    atomic_uint opens,closes;
    atomic_int guardian;
} SummaryOwnedFixture;
static inline void owned_init(SummaryOwnedFixture *host,const char *self,const char *root){
    memset(host,0,sizeof(*host));host->process=(EngineProcess){.guardian=-1,.control=-1,.response=-1};
    host->self=self;host->write_fd=host->read_fd=-1;atomic_init(&host->opens,0);atomic_init(&host->closes,0);atomic_init(&host->guardian,0);
    snprintf(host->lease,sizeof(host->lease),"%s/residency",root);
    snprintf(host->marker,sizeof(host->marker),"%s/marker",root);snprintf(host->log,sizeof(host->log),"%s/engine.log",root);
}
static inline const char *owned_close(void *context){
    SummaryOwnedFixture *host=context;
    atomic_fetch_add(&host->closes,1);
    if(host->process.guardian>0||host->process.response>=0){
        const char *error=engine_process_stop(&host->process);
        if(error&&!strcmp(error,"engine_shutdown_pending"))return error;
    }
    if(host->write_fd>=0)close(host->write_fd);
    if(host->read_fd>=0)close(host->read_fd);
    host->write_fd=host->read_fd=-1;atomic_store(&host->guardian,0);return NULL;
}
static inline const char *owned_open(void *context,const SamosaSummaryProcessConfig *config,
        const SamosaSummaryControl *control,int *write_fd,int *read_fd){
    SummaryOwnedFixture *host=context;
    if(control&&control->check){const char *error=control->check(control->context);if(error)return error;}
    if(host->process.guardian>0){
        struct pollfd watch={host->process.response,POLLIN|POLLHUP,0};
        if(poll(&watch,1,0)!=0)return "owned_engine_exited";
    }else{
        int input[2]={-1,-1},output[2]={-1,-1};
        int lease=open(host->lease,O_RDWR|O_CREAT|O_CLOEXEC,0600),model=open(config->model,O_RDONLY|O_CLOEXEC);
        const char *error=NULL;
        if(lease<0||model<0||flock(lease,LOCK_EX|LOCK_NB)||pipe(input)||pipe(output))error="fixture_io_failed";
        int descriptors[]={input[0],input[1],output[0],output[1]};
        for(size_t i=0;!error&&i<4;i++)if(fcntl(descriptors[i],F_SETFD,FD_CLOEXEC))error="fixture_io_failed";
        char *body=NULL;size_t bytes=0;FILE *out=!error?open_memstream(&body,&bytes):NULL;
        if(!error&&!out)error="fixture_io_failed";
        if(out){
            fputc('[',out);json_quote(out,config->engine,strlen(config->engine));
            fputs(",\"--model\",\"/dev/fd/5\",\"--gpu-layers\",",out);json_quote(out,config->gpu_layers,strlen(config->gpu_layers));
            fputs(",\"--max-tokens\",\"128\"",out);
            if(host->mode){
                fputs(",\"--test-summary-mode\",",out);json_quote(out,host->mode,strlen(host->mode));
                fputs(",\"--test-summary-marker\",",out);json_quote(out,host->marker,strlen(host->marker));
                fputs(",\"--test-summary-log\",",out);json_quote(out,host->log,strlen(host->log));
            }
            fputc(']',out);if(fclose(out))error="fixture_io_failed";
        }
        if(!error&&host->workspace[0]){
            int directory=open(host->workspace,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
            if(directory<0)error="fixture_io_failed";
            else{error=engine_process_start_bound_workspace_duplex(host->self,body,300,1048576,lease,model,host->workspace,directory,input[0],output[1],&host->process);close(directory);}
        }else if(!error)error=engine_process_start_bound_duplex(host->self,body,300,1048576,lease,model,input[0],output[1],&host->process);
        free(body);if(lease>=0)close(lease);if(model>=0)close(model);
        if(input[0]>=0)close(input[0]);if(output[1]>=0)close(output[1]);
        host->write_fd=input[1];host->read_fd=output[0];
        if(error){const char *cleanup=owned_close(host);return cleanup?cleanup:error;}
        atomic_fetch_add(&host->opens,1);atomic_store(&host->guardian,host->process.guardian);
    }
    *write_fd=host->write_fd;*read_fd=host->read_fd;return NULL;
}
static inline int owned_released(const SummaryOwnedFixture *host){
    int fd=open(host->lease,O_RDWR|O_CLOEXEC);if(fd<0)return 0;
    int released=!flock(fd,LOCK_EX|LOCK_NB);close(fd);return released;
}
#endif

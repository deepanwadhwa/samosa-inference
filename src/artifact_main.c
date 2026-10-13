#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "artifact.h"
#include "json.h"
#include <errno.h>
#include <stdint.h>
#ifdef SAMOSA_REMOTE_ARTIFACTS
#include "remote_artifact.h"
#include "private_key.h"
#endif

static int number(const char *text,uint64_t *result) {
    if(!text || !*text || text[0]<'0' || text[0]>'9')return 0;
    char *end;errno=0;unsigned long long n=strtoull(text,&end,10);
    if(errno || *end)return 0;
    *result=n;return 1;
}
int main(int argc,char **argv) {
    ArtifactConfig c={0};
#ifdef SAMOSA_REMOTE_ARTIFACTS
    RemoteArtifactSource remote={0};
    const char *key_path=NULL;char key_bytes[130]={0};
#endif
    for(int i=1;i<argc;i++) {
        if(i+1>=argc)return 2;
        const char *key=argv[i],*value=argv[++i];
        if(!strcmp(key,"--state"))c.state_path=value;
        else if(!strcmp(key,"--source"))c.source_path=value;
        else if(!strcmp(key,"--sha256"))c.sha256=value;
        else if(!strcmp(key,"--size")){if(!number(value,&c.size))return 2;}
        else if(!strcmp(key,"--byte-ceiling")){if(!number(value,&c.byte_ceiling))return 2;}
        else if(!strcmp(key,"--disk-ceiling")){if(!number(value,&c.disk_ceiling))return 2;}
#ifdef SAMOSA_REMOTE_ARTIFACTS
        else if(!strcmp(key,"--url"))remote.url=value;
        else if(!strcmp(key,"--ca-file"))remote.ca_file=value;
        else if(!strcmp(key,"--bearer-key-file"))key_path=value;
        else if(!strcmp(key,"--redirect-host")){if(remote.redirect_count==8)return 2;remote.redirect_hosts[remote.redirect_count++]=value;}
#endif
        else return 2;
    }
#ifdef SAMOSA_REMOTE_ARTIFACTS
    if(key_path){if(!remote.url||!private_key_read(key_path,key_bytes))return 2;remote.bearer_key=key_bytes;}
#endif
    char request[4097];size_t n=fread(request,1,sizeof(request),stdin);
    Json j;size_t version;char *op=NULL;const char *error=NULL,*status=NULL;
    if(ferror(stdin)||n>4096)error="request_too_large";
    else if(!json_parse(&j,request,n)||j.tokens[0].type!=JT_OBJECT)error="invalid_json";
    else if(!json_uint(&j,json_get(&j,0,"contract_version"),&version)||version!=1)error="unsupported_contract";
    else if(j.tokens[0].count!=2)error="invalid_request";
    else if(!(op=json_string(&j,json_get(&j,0,"op"))))error="invalid_request";
    else if(!strcmp(op,"artifact_status"))error=artifact_status(&c,&status);
    else if(!strcmp(op,"artifact_make_executable")){error=artifact_make_executable(&c);if(!error)status="verified";}
    else if(!strcmp(op,"artifact_install")||!strcmp(op,"artifact_resume")){
        c.resume=!strcmp(op,"artifact_resume");
#ifdef SAMOSA_REMOTE_ARTIFACTS
        if(remote.url&&c.source_path)error="invalid_remote_artifact_configuration";
        else error=remote.url?artifact_install_remote(&c,&remote):artifact_install(&c);
#else
        error=artifact_install(&c);
#endif
        if(!error)status="verified";
    } else error="unsupported_operation";
    fputs("{\"contract_version\":1",stdout);
    if(status){fputs(",\"status\":",stdout);json_quote(stdout,status,strlen(status));}
    if(error){fputs(",\"error\":{\"code\":",stdout);json_quote(stdout,error,strlen(error));fputc('}',stdout);}
    else {fputs(",\"artifact_id\":",stdout);json_quote(stdout,c.sha256,strlen(c.sha256));
        fprintf(stdout,",\"size\":%llu,\"activation\":\"not_requested\",\"readiness\":\"unqualified\"",(unsigned long long)c.size);}
    fputs("}\n",stdout);free(op);
#ifdef SAMOSA_REMOTE_ARTIFACTS
    private_key_clear(key_bytes);
#endif
    return error?1:0;
}

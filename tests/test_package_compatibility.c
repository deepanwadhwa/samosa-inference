#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "package_compatibility.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"compatibility failure %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static const char *cancel(void *unused){(void)unused;return "cancelled";}
int main(int argc,char **argv){
    PackageIndex *index=calloc(1,sizeof(*index));CHECK(index);
    PackageCompatibilityContext context={.payload_path="/unused/private-payload"};
    if(argc==4){
        int file=open(argv[2],O_RDONLY|O_NOFOLLOW);struct stat s;CHECK(file>=0&&!fstat(file,&s));
        CHECK(S_ISREG(s.st_mode)&&s.st_uid==geteuid()&&!(s.st_mode&077)&&s.st_nlink==1&&s.st_size>0&&s.st_size<=PACKAGE_INDEX_BYTES);
        char *body=malloc((size_t)s.st_size);CHECK(body);size_t at=0;
        while(at<(size_t)s.st_size){ssize_t n=read(file,body+at,(size_t)s.st_size-at);CHECK(n>0);at+=(size_t)n;}
        CHECK(!close(file));CHECK(!package_index_parse(body,at,argv[3],index));free(body);
        context.payload_path=argv[1];int root=open(argv[1],O_RDONLY|O_DIRECTORY|O_NOFOLLOW);CHECK(root>=0);
        const char *e=package_backend_compatible(root,index,&context);if(e)fprintf(stderr,"backend compatibility: %s\n",e);CHECK(!e);
        context.control=cancel;CHECK(!strcmp(package_backend_compatible(root,index,&context),"cancelled"));
        CHECK(!close(root));printf("PASS pinned native speech package compatibility: index %s; closed manifest/graph/host inspection, no model load or readiness claim\n",index->manifest_sha256);free(index);return 0;
    }
    CHECK(argc==1);index->count=1;strcpy(index->recipe,"unknown-v1");
    CHECK(!strcmp(package_backend_compatible(-1,index,&context),"unsupported_package_recipe"));
    strcpy(index->recipe,"sherpa-speech-package-v1");CHECK(!strcmp(package_backend_compatible(-1,index,&context),"invalid_speech_package_index"));
    strcpy(index->files[0].name,"manifest.json");index->files[0].role=PACKAGE_MODEL;
    CHECK(!strcmp(package_backend_compatible(-1,index,&context),"invalid_speech_package_index"));
    index->files[0].role=PACKAGE_METADATA;context.control=cancel;
    CHECK(!strcmp(package_backend_compatible(-1,index,&context),"cancelled"));
    context.payload_path="relative";CHECK(!strcmp(package_backend_compatible(-1,index,&context),"invalid_package_compatibility"));
    free(index);printf("PASS package recipe dispatch: %u checks; unknown recipe, manifest role, control and private context refusal\n",checks);return 0;
}

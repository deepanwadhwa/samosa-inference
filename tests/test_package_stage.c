#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "package_stage.h"
#include "package_tree.h"
#include "sha256.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"stage failure %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static const char *cancel(void *p){(void)p;return "cancelled";}
typedef struct {int root;const char *path;} CopyCancel;
static const char *cancel_copy(void *p){CopyCancel *c=p;return !faccessat(c->root,c->path,F_OK,0)?"cancelled":NULL;}
int main(void){
    char base[]="/tmp/samosa-package-stage.XXXXXX";CHECK(mkdtemp(base));char root_path[256],source_path[256];
    snprintf(root_path,sizeof(root_path),"%s/payload",base);snprintf(source_path,sizeof(source_path),"%s/source",base);
    CHECK(!mkdir(root_path,0700));int root=open(root_path,O_RDONLY|O_DIRECTORY);CHECK(root>=0);
    int writer=open(source_path,O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(writer>=0);CHECK(write(writer,"abc",3)==3);CHECK(!close(writer));
    int input=open(source_path,O_RDONLY);CHECK(input>=0);CHECK(lseek(input,1,SEEK_SET)==1);
    char source_alias[256];snprintf(source_alias,sizeof(source_alias),"%s/source-alias",base);CHECK(!link(source_path,source_alias));
    const char *body="{\"format\":\"samosa.inference.package.v1\",\"platform\":\"macos-arm64\",\"minimum_os\":\"14.0\",\"recipe\":\"fixture-v1\",\"files\":[{\"name\":\"models/model.gguf\",\"role\":\"model\",\"bytes\":3,\"sha256\":\"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\"}]}";
    PackageIndex *index=malloc(sizeof(*index));CHECK(index);char pin[65];hash_bytes(body,strlen(body),pin);CHECK(!package_index_parse(body,strlen(body),pin,index));
    CHECK(!strcmp(package_stage_file(root,index,0,input,cancel,NULL),"cancelled"));CHECK(faccessat(root,"models",F_OK,0)<0);
    CHECK(!package_stage_file(root,index,0,input,NULL,NULL));CHECK(lseek(input,0,SEEK_CUR)==1);
    struct stat s;CHECK(!fstatat(root,"models/model.gguf",&s,0));CHECK((s.st_mode&0777)==0400&&s.st_nlink==1);
    PackageTreeResult result;CHECK(!package_tree_verify(root,index,NULL,NULL,&result));CHECK(result.files==1&&result.payload_bytes==3);
    CHECK(!package_stage_file(root,index,0,input,NULL,NULL));CHECK(!package_tree_verify(root,index,NULL,NULL,&result));
    CHECK(!unlinkat(root,"models/model.gguf",0));
    int parent=openat(root,"models",O_RDONLY|O_DIRECTORY);CHECK(parent>=0);char temporary[80];snprintf(temporary,sizeof(temporary),".copy-0-%s",index->files[0].sha256);
    writer=openat(parent,temporary,O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(writer>=0);CHECK(write(writer,"foreign",7)==7);CHECK(!close(writer));
    CHECK(!strcmp(package_stage_file(root,index,0,input,NULL,NULL),"package_stage_busy"));CHECK(!fstatat(parent,temporary,&s,0)&&s.st_size==7);
    CHECK(!unlinkat(parent,temporary,0));
    char partial[128];snprintf(partial,sizeof(partial),"models/%s",temporary);CopyCancel interrupted={root,partial};
    CHECK(!strcmp(package_stage_file(root,index,0,input,cancel_copy,&interrupted),"cancelled"));
    CHECK(faccessat(parent,temporary,F_OK,0)<0&&faccessat(parent,"model.gguf",F_OK,0)<0);
    CHECK(!symlinkat(source_path,parent,"model.gguf"));CHECK(!strcmp(package_stage_file(root,index,0,input,NULL,NULL),"unsafe_package_stage"));CHECK(!unlinkat(parent,"model.gguf",0));
    writer=openat(parent,"model.gguf",O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(writer>=0);CHECK(write(writer,"abd",3)==3);CHECK(!close(writer));
    CHECK(!strcmp(package_stage_file(root,index,0,input,NULL,NULL),"artifact_checksum_mismatch"));CHECK(!fstatat(parent,"model.gguf",&s,0)&&s.st_size==3);
    CHECK(!unlinkat(parent,"model.gguf",0));CHECK(!package_stage_file(root,index,0,input,NULL,NULL));
    size_t large_size=131081;unsigned char *large=malloc(large_size);CHECK(large);
    for(size_t i=0;i<large_size;i++)large[i]=(unsigned char)(i%251);
    writer=open(source_path,O_WRONLY|O_TRUNC);CHECK(writer>=0);CHECK(write(writer,large,large_size)==(ssize_t)large_size);CHECK(!close(writer));
    char large_sha[65],large_index[1024];hash_bytes(large,large_size,large_sha);free(large);
    snprintf(large_index,sizeof(large_index),"{\"format\":\"samosa.inference.package.v1\",\"platform\":\"macos-arm64\",\"minimum_os\":\"14.0\",\"recipe\":\"fixture-v1\",\"files\":[{\"name\":\"models/model.gguf\",\"role\":\"model\",\"bytes\":%zu,\"sha256\":\"%s\"}]}",large_size,large_sha);
    hash_bytes(large_index,strlen(large_index),pin);CHECK(!package_index_parse(large_index,strlen(large_index),pin,index));
    CHECK(!strcmp(package_stage_file(root,index,0,input,NULL,NULL),"artifact_size_mismatch"));
    CHECK(!unlinkat(parent,"model.gguf",0));CHECK(!package_stage_file(root,index,0,input,NULL,NULL));
    CHECK(lseek(input,0,SEEK_CUR)==1);CHECK(!package_tree_verify(root,index,NULL,NULL,&result));CHECK(result.payload_bytes==large_size);
    CHECK(!unlinkat(parent,"model.gguf",0));CHECK(!close(parent));CHECK(!unlinkat(root,"models",AT_REMOVEDIR));CHECK(!close(root));CHECK(!close(input));
    CHECK(!unlink(source_path));CHECK(!unlink(source_alias));CHECK(!rmdir(root_path));CHECK(!rmdir(base));free(index);
    printf("PASS package staging: %u checks; pinned copies, source seek preservation, closed tree, no replacement/adoption and cancellation\n",checks);return 0;
}

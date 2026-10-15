#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "molmo_package.h"
#include "library_runtime.h"
#include "catalog.h"
#include "library_fixture.h"
#include "sha256.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"Molmo package failure %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static const char *cancel(void *p){(void)p;return "cancelled";}
static void put(int root,PackageIndex *index,size_t i,const void *body,size_t bytes){
    PackageFile *f=&index->files[i];int fd=openat(root,f->name,O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW,0600);CHECK(fd>=0);CHECK(write(fd,body,bytes)==(ssize_t)bytes);CHECK(!close(fd));index->payload_bytes-=f->bytes;f->bytes=bytes;index->payload_bytes+=bytes;hash_bytes(body,bytes,f->sha256);
}
static void rejects(int root,const char *path,PackageIndex *index,const MolmoPackagePolicy *policy,const char *expected){
    MolmoPackageDescriptor out;memset(&out,0xa5,sizeof(out));MolmoPackageDescriptor before=out;const char *e=molmo_package_verify(root,path,index,policy,NULL,NULL,&out);
    if(!e||strcmp(e,expected))fprintf(stderr,"expected %s got %s\n",expected,e?e:"success");CHECK(e&&!strcmp(e,expected));CHECK(!memcmp(&out,&before,sizeof(out)));
}
int main(int argc,char **argv){
    if(argc==8&&!strcmp(argv[1],"--real")){
        FILE *f=fopen(argv[3],"rb");CHECK(f);CHECK(!fseek(f,0,SEEK_END));long bytes=ftell(f);CHECK(bytes>0&&bytes<=PACKAGE_INDEX_BYTES);rewind(f);char *body=malloc((size_t)bytes);CHECK(body);CHECK(fread(body,1,(size_t)bytes,f)==(size_t)bytes);CHECK(!fclose(f));
        PackageIndex *index=calloc(1,sizeof(*index));CHECK(index);CHECK(!package_index_parse(body,(size_t)bytes,argv[4],index));free(body);
        int root=open(argv[2],O_RDONLY|O_DIRECTORY|O_NOFOLLOW);CHECK(root>=0);MolmoPackagePolicy policy={argv[5],argv[6],argv[7]};MolmoPackageDescriptor out;
        const char *e=molmo_package_verify(root,argv[2],index,&policy,NULL,NULL,&out);if(e)fprintf(stderr,"actual Molmo package: %s\n",e);CHECK(!e);CHECK(!close(root));free(index);
        printf("PASS actual Molmo static package admission: %u checks; closed native runtime and exact existing temporal model manifest/index binding; no readiness claim\n",checks);return 0;
    }
    CHECK(argc==1);char path[]="/tmp/samosa-molmo-package-XXXXXX";CHECK(mkdtemp(path));int root=open(path,O_RDONLY|O_DIRECTORY);CHECK(root>=0);CHECK(!mkdirat(root,"model",0700));
    PackageIndex *index=calloc(1,sizeof(*index));CHECK(index);index->count=8;memset(index->manifest_sha256,'a',64);strcpy(index->recipe,"molmo2-mlx-package-v1");strcpy(index->minimum_os,"1.0");char os[65];CHECK(!provider_catalog_host(index->platform,os));
    const char *names[]={"engine","mlx.metallib","molmo-package.json","model/manifest.json","model/config.json","model/tokenizer.json","model/processor.json","model/weights.safetensors"};
    const enum PackageFileRole roles[]={PACKAGE_ENGINE,PACKAGE_DATA,PACKAGE_METADATA,PACKAGE_METADATA,PACKAGE_METADATA,PACKAGE_DATA,PACKAGE_METADATA,PACKAGE_MODEL};
    for(size_t i=0;i<8;i++){strcpy(index->files[i].name,names[i]);index->files[i].role=roles[i];}
    LibraryRuntimePolicy host;CHECK(!library_runtime_host_policy(&host));unsigned char image[4096];if(host.mach_o)lib_executable_mach(image,host.cpu);else lib_executable_elf(image,host.cpu);put(root,index,0,image,sizeof(image));
    for(size_t i=1;i<8;i++)put(root,index,i,"{}",2);
    const char *body="{\"format\":\"samosa.inference.molmo-package.v1\",\"profile\":\"molmo2-4b-mlx-q4-v1\",\"engine\":\"engine\",\"model_directory\":\"model\"}";put(root,index,2,body,strlen(body));
    MolmoPackagePolicy policy={index->files[0].sha256,index->files[1].sha256,index->files[3].sha256};CHECK(molmo_package_policy_valid(&policy));
    rejects(-1,path,index,NULL,"invalid_molmo_package_policy");rejects(-1,path,index,&policy,"unsafe_molmo_package_root");
    MolmoPackageDescriptor out;memset(&out,0xa5,sizeof(out));MolmoPackageDescriptor before=out;CHECK(!strcmp(molmo_package_verify(-1,path,index,&policy,cancel,NULL,&out),"cancelled"));CHECK(!memcmp(&out,&before,sizeof(out)));
    const char **pins[]={&policy.engine_sha256,&policy.kernels_sha256,&policy.manifest_sha256};for(size_t i=0;i<3;i++){const char *old=*pins[i];*pins[i]="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";rejects(root,path,index,&policy,i==2?"unqualified_molmo_package_model":"unqualified_molmo_package_runtime");*pins[i]=old;}
    /* A hash-consistent synthetic payload still cannot bypass native model
     * profile checks. Actual success requires the separately supplied real gate. */
    rejects(root,path,index,&policy,"invalid_model_package");
    CHECK(!unlinkat(root,"mlx.metallib",0));rejects(root,path,index,&policy,"package_file_missing");put(root,index,1,"{}",2);
    CHECK(!symlinkat("model",root,"foreign"));rejects(root,path,index,&policy,"unsupported_package_artifact");CHECK(!unlinkat(root,"foreign",0));
    rejects(root,path,index,&policy,"invalid_model_package");
    for(size_t i=0;i<8;i++)CHECK(!unlinkat(root,index->files[i].name,0));CHECK(!unlinkat(root,"model",AT_REMOVEDIR));free(index);CHECK(!close(root));CHECK(!rmdir(path));
    printf("PASS Molmo native admission refusal: %u checks; independent policy, bound root, closed tree, missing kernel, native model profile and controls; actual model gate separate\n",checks);return 0;
}

#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "package_compatibility.h"
#include "package_stage.h"
#include "package_assemble.h"
#include "artifact.h"
#include "package_cache.h"
#include <sys/file.h>
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
    if(argc==4||argc==5){
        int file=open(argv[2],O_RDONLY|O_NOFOLLOW);struct stat s;CHECK(file>=0&&!fstat(file,&s));
        CHECK(S_ISREG(s.st_mode)&&s.st_uid==geteuid()&&!(s.st_mode&077)&&s.st_nlink==1&&s.st_size>0&&s.st_size<=PACKAGE_INDEX_BYTES);
        char *body=malloc((size_t)s.st_size);CHECK(body);size_t at=0;
        while(at<(size_t)s.st_size){ssize_t n=read(file,body+at,(size_t)s.st_size-at);CHECK(n>0);at+=(size_t)n;}
        CHECK(!close(file));CHECK(!package_index_parse(body,at,argv[3],index));free(body);
        context.payload_path=argv[1];int root=open(argv[1],O_RDONLY|O_DIRECTORY|O_NOFOLLOW);CHECK(root>=0);
        const char *e=package_backend_compatible(root,index,&context);if(e)fprintf(stderr,"backend compatibility: %s\n",e);CHECK(!e);
        if(argc==5){
            /* Exercise the same verified copies and concrete compatibility gate
             * a coordinator must use; this harness owns the private destination. */
            int directory=open(argv[4],O_RDONLY|O_DIRECTORY|O_NOFOLLOW);CHECK(directory>=0);
            char destination[4096];CHECK(snprintf(destination,sizeof(destination),"%s/payload",argv[4])<(int)sizeof(destination));
            PackageAssembly assembly={.directory=directory,.source=root,.index=index,.payload_path=destination,.copy_ceiling=index->payload_bytes};
            PackageTreeResult observed;
            assembly.copy_ceiling=index->payload_bytes-1;
            CHECK(!strcmp(package_assemble(&assembly,&observed),"package_copy_quota_exceeded"));
            CHECK(faccessat(directory,"payload",F_OK,0)<0);
            assembly.copy_ceiling=index->payload_bytes;
            int lease=openat(directory,"assemble.lock",O_RDWR|O_CREAT|O_NOFOLLOW,0600);CHECK(lease>=0);CHECK(!flock(lease,LOCK_EX|LOCK_NB));
            CHECK(!strcmp(package_assemble(&assembly,&observed),"package_assembly_busy"));CHECK(!close(lease));
            /* A retained verified file survives resumption without replacement. */
            CHECK(!mkdirat(directory,"payload",0700));
            int retained=openat(directory,"payload",O_RDONLY|O_DIRECTORY|O_NOFOLLOW);CHECK(retained>=0);
            int first=openat(root,index->files[0].name,O_RDONLY|O_NOFOLLOW);CHECK(first>=0);
            CHECK(!package_stage_file(retained,index,0,first,NULL,NULL));CHECK(!close(first));
            struct stat before,after;CHECK(!fstatat(retained,index->files[0].name,&before,AT_SYMLINK_NOFOLLOW));
            CHECK(!package_assemble(&assembly,&observed));CHECK(observed.files==index->count);
            CHECK(!fstatat(retained,index->files[0].name,&after,AT_SYMLINK_NOFOLLOW));CHECK(before.st_ino==after.st_ino&&before.st_dev==after.st_dev);CHECK(!close(retained));
            CHECK(!package_assemble(&assembly,&observed));
            /* A second package assembles directly from the registered artifact
             * store, whose stage/artifact cache names share two links. */
            char store[4096],cache_path[4096];CHECK(snprintf(store,sizeof(store),"%s/artifacts",argv[4])<(int)sizeof(store));
            ArtifactConfig cache={.state_path=store,.byte_ceiling=index->payload_bytes,.disk_ceiling=index->payload_bytes*2,.artifact_capacity=PACKAGE_INDEX_FILES};
            for(size_t i=0;i<index->count;i++){
                char source[4096];CHECK(snprintf(source,sizeof(source),"%s/%s",argv[1],index->files[i].name)<(int)sizeof(source));
                cache.source_path=source;cache.sha256=index->files[i].sha256;cache.size=index->files[i].bytes;CHECK(!artifact_install(&cache));
            }
            CHECK(!mkdirat(directory,"cached-package",0700));int cache_directory=openat(directory,"cached-package",O_RDONLY|O_DIRECTORY|O_NOFOLLOW);CHECK(cache_directory>=0);
            CHECK(snprintf(cache_path,sizeof(cache_path),"%s/cached-package/payload",argv[4])<(int)sizeof(cache_path));
            PackageCacheSource cache_source={.store=cache};
            PackageAssembly from_cache={.directory=cache_directory,.source=-1,.source_open=package_cache_open,.source_context=&cache_source,.index=index,.payload_path=cache_path,.copy_ceiling=index->payload_bytes};
            CHECK(!package_assemble(&from_cache,&observed));CHECK(observed.files==index->count);CHECK(!package_assemble(&from_cache,&observed));CHECK(!close(cache_directory));
            /* Exercise cache misses through real local acquisition instead of
             * requiring a caller to populate every identity in advance. */
            CHECK(!mkdirat(directory,"acquired-package",0700));int acquired=openat(directory,"acquired-package",O_RDONLY|O_DIRECTORY|O_NOFOLLOW);CHECK(acquired>=0);
            char acquisition_store[4096],acquisition_path[4096];CHECK(snprintf(acquisition_store,sizeof(acquisition_store),"%s/acquired-artifacts",argv[4])<(int)sizeof(acquisition_store));
            CHECK(snprintf(acquisition_path,sizeof(acquisition_path),"%s/acquired-package/payload",argv[4])<(int)sizeof(acquisition_path));
            PackageCacheSource local=cache_source;local.store.state_path=acquisition_store;local.source_directory=argv[1];
            PackageAssembly acquisition=from_cache;acquisition.directory=acquired;acquisition.payload_path=acquisition_path;acquisition.source_context=&local;
            int miss=-7;CHECK(!strcmp(package_cache_open(&local,&index->files[0],cancel,NULL,&miss),"cancelled"));CHECK(miss==-7);CHECK(access(acquisition_store,F_OK)<0);
            CHECK(!package_assemble(&acquisition,&observed));CHECK(observed.files==index->count);
            local.source_directory="/unavailable-local-source";int offline=-1;CHECK(!package_cache_open(&local,&index->files[0],NULL,NULL,&offline));CHECK(offline>=0);CHECK(!close(offline));CHECK(!package_assemble(&acquisition,&observed));CHECK(!close(acquired));
            char bad_cache[8192];CHECK(snprintf(bad_cache,sizeof(bad_cache),"%s/%s/artifact",store,index->files[0].sha256)<(int)sizeof(bad_cache));
            int broken=open(bad_cache,O_WRONLY|O_NOFOLLOW);CHECK(broken>=0);CHECK(pwrite(broken,"X",1,0)==1);CHECK(!close(broken));
            CHECK(!mkdirat(directory,"rejected-package",0700));int rejected=openat(directory,"rejected-package",O_RDONLY|O_DIRECTORY|O_NOFOLLOW);CHECK(rejected>=0);
            char rejected_path[4096];CHECK(snprintf(rejected_path,sizeof(rejected_path),"%s/rejected-package/payload",argv[4])<(int)sizeof(rejected_path));
            PackageAssembly rejected_assembly=from_cache;rejected_assembly.directory=rejected;rejected_assembly.payload_path=rejected_path;
            CHECK(!strcmp(package_assemble(&rejected_assembly,&observed),"artifact_checksum_mismatch"));CHECK(faccessat(rejected,"complete",F_OK,0)<0);CHECK(!close(rejected));
            /* Completed package verification is independent of cache survival. */
            cache_directory=openat(directory,"cached-package",O_RDONLY|O_DIRECTORY|O_NOFOLLOW);CHECK(cache_directory>=0);from_cache.directory=cache_directory;
            CHECK(!package_assemble(&from_cache,&observed));CHECK(!close(cache_directory));
            int payload=openat(directory,"payload",O_RDONLY|O_DIRECTORY|O_NOFOLLOW);CHECK(payload>=0);
            PackageCompatibilityContext staged={.payload_path=destination};
            PackagePublication publication={.directory=directory,.index=index,.compatible=package_backend_compatible,.compatibility=&staged};
            CHECK(!package_publication_verify(&publication,&observed));
            CHECK(!package_publish(&publication,&observed));
            /* Revocation remains effective even after a completion record exists. */
            assembly.control=cancel;CHECK(!strcmp(package_assemble(&assembly,&observed),"cancelled"));
            staged.control=cancel;const char *failure=package_publication_verify(&publication,&observed);
            CHECK(failure&&!strcmp(failure,"cancelled"));
            assembly.control=NULL;
            int changed=openat(payload,index->files[0].name,O_RDONLY|O_NOFOLLOW);CHECK(changed>=0);CHECK(!fchmod(changed,0600));CHECK(!close(changed));
            changed=openat(payload,index->files[0].name,O_WRONLY|O_NOFOLLOW);CHECK(changed>=0);CHECK(pwrite(changed,"X",1,0)==1);CHECK(!close(changed));
            CHECK(package_assemble(&assembly,&observed));
            changed=openat(payload,index->files[0].name,O_RDONLY|O_NOFOLLOW);CHECK(changed>=0);char byte;CHECK(pread(changed,&byte,1,0)==1&&byte=='X');CHECK(!close(changed));
            CHECK(!close(payload));CHECK(!close(directory));
            printf("PASS staged native speech publication: %zu files; compatibility, completion, revalidation and cancellation\n",index->count);
        }
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

#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "package_compatibility.h"
#include "package_stage.h"
#include "package_assemble.h"
#include "artifact.h"
#include "package_cache.h"
#include "package_catalog.h"
#ifdef SAMOSA_PACKAGE_REMOTE_TESTS
#include "package_remote_cache.h"
#include "package_installer.h"
#include "package_reserve.h"
#endif
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
#ifdef SAMOSA_PACKAGE_REMOTE_TESTS
static const char *reservation_interrupt(void *opaque){int pool=*(int *)opaque;struct stat s;
    return !fstatat(pool,".reservation.pending",&s,AT_SYMLINK_NOFOLLOW)?"deadline_exceeded":NULL;
}
#endif
int main(int argc,char **argv){
    PackageIndex *index=calloc(1,sizeof(*index));CHECK(index);
    PackageCompatibilityContext context={.payload_path="/unused/private-payload"};
    if(argc==4||argc==5||argc==7){
        int file=open(argv[2],O_RDONLY|O_NOFOLLOW);struct stat s;CHECK(file>=0&&!fstat(file,&s));
        CHECK(S_ISREG(s.st_mode)&&s.st_uid==geteuid()&&!(s.st_mode&077)&&s.st_nlink==1&&s.st_size>0&&s.st_size<=PACKAGE_INDEX_BYTES);
        char *body=malloc((size_t)s.st_size);CHECK(body);size_t at=0;
        while(at<(size_t)s.st_size){ssize_t n=read(file,body+at,(size_t)s.st_size-at);CHECK(n>0);at+=(size_t)n;}
        CHECK(!close(file));CHECK(!package_index_parse(body,at,argv[3],index));free(body);
        /* Reuse the trusted catalog's existing artifact descriptor contract:
         * declared package policy must agree with the pinned index bytes. */
        ProviderEntry entry={.size=at};strcpy(entry.sha256,argv[3]);strcpy(entry.runtime,"system-runtime-v1");strcpy(entry.platform,index->platform);strcpy(entry.minimum_os,index->minimum_os);strcpy(entry.recipe,index->recipe);
        file=open(argv[2],O_RDONLY|O_NOFOLLOW);CHECK(file>=0);CHECK(lseek(file,1,SEEK_SET)==1);
        PackageIndex *catalog_index=calloc(1,sizeof(*catalog_index));CHECK(catalog_index);
        CHECK(!package_catalog_read_index(&entry,file,NULL,NULL,catalog_index));CHECK(!strcmp(catalog_index->manifest_sha256,index->manifest_sha256));CHECK(catalog_index->count==index->count);CHECK(lseek(file,0,SEEK_CUR)==1);
        strcpy(entry.recipe,"another-recipe");CHECK(!strcmp(package_catalog_read_index(&entry,file,NULL,NULL,catalog_index),"package_catalog_identity_conflict"));CHECK(!strcmp(catalog_index->recipe,index->recipe));strcpy(entry.recipe,index->recipe);
        CHECK(!strcmp(package_catalog_read_index(&entry,file,cancel,NULL,catalog_index),"cancelled"));CHECK(!close(file));free(catalog_index);
        context.payload_path=argv[1];int root=open(argv[1],O_RDONLY|O_DIRECTORY|O_NOFOLLOW);CHECK(root>=0);
        const char *e=package_backend_compatible(root,index,&context);if(e)fprintf(stderr,"backend compatibility: %s\n",e);CHECK(!e);
        if(argc>=5){
            /* Exercise the same verified copies and concrete compatibility gate
             * a coordinator must use; this harness owns the private destination. */
            int directory=open(argv[4],O_RDONLY|O_DIRECTORY|O_NOFOLLOW);CHECK(directory>=0);
            char destination[4096];CHECK(snprintf(destination,sizeof(destination),"%s/payload",argv[4])<(int)sizeof(destination));
            PackageAssembly assembly={.directory=directory,.source=root,.index=index,.payload_path=destination,.copy_ceiling=index->payload_bytes};
            PackageTreeResult observed;PackageAssemblyStatus status;
            CHECK(!package_assembly_status(&assembly,&status));CHECK(!strcmp(status.state,"unregistered"));CHECK(faccessat(directory,"assemble.lock",F_OK,0)<0);
            assembly.copy_ceiling=index->payload_bytes-1;
            CHECK(!strcmp(package_assemble(&assembly,&observed),"package_copy_quota_exceeded"));
            CHECK(faccessat(directory,"payload",F_OK,0)<0);
            assembly.copy_ceiling=index->payload_bytes;
            int lease=openat(directory,"assemble.lock",O_RDWR|O_CREAT|O_NOFOLLOW,0600);CHECK(lease>=0);CHECK(!flock(lease,LOCK_EX|LOCK_NB));
            CHECK(!package_assembly_status(&assembly,&status));CHECK(!strcmp(status.state,"busy"));CHECK(!status.package.files);
            CHECK(!strcmp(package_assemble(&assembly,&observed),"package_assembly_busy"));CHECK(!close(lease));
            /* A retained verified file survives resumption without replacement. */
            CHECK(!mkdirat(directory,"payload",0700));
            int retained=openat(directory,"payload",O_RDONLY|O_DIRECTORY|O_NOFOLLOW);CHECK(retained>=0);
            int first=openat(root,index->files[0].name,O_RDONLY|O_NOFOLLOW);CHECK(first>=0);
            CHECK(!package_stage_file(retained,index,0,first,NULL,NULL));CHECK(!close(first));
            struct stat before,after;CHECK(!fstatat(retained,index->files[0].name,&before,AT_SYMLINK_NOFOLLOW));
            const char *assembled=package_assemble(&assembly,&observed);if(assembled)fprintf(stderr,"initial complete assembly: %s\n",assembled);CHECK(!assembled);CHECK(observed.files==index->count);
            CHECK(!package_assembly_status(&assembly,&status));CHECK(!strcmp(status.state,"installed_verified"));CHECK(status.package.files==index->count);
            PackageAssembly inspection=assembly;inspection.source=-1;inspection.source_open=NULL;inspection.copy_ceiling=0;
            CHECK(!package_assembly_status(&inspection,&status));CHECK(!strcmp(status.state,"installed_verified"));
            inspection.copy_ceiling=assembly.copy_ceiling;
            CHECK(!renameat(directory,"complete",directory,".complete.pending"));
            CHECK(!package_assemble(&inspection,&observed));CHECK(!package_assembly_status(&inspection,&status));CHECK(!strcmp(status.state,"installed_verified"));
            CHECK(!linkat(directory,"complete",directory,".complete.pending",0));
            CHECK(!strcmp(package_assembly_status(&inspection,&status),"package_publication_recovery_required"));
            CHECK(!package_assemble(&inspection,&observed));CHECK(!package_assembly_status(&inspection,&status));
            inspection.control=cancel;memset(&status,0xa5,sizeof(status));PackageAssemblyStatus cancelled_status=status;
            CHECK(!strcmp(package_assembly_status(&inspection,&status),"cancelled"));CHECK(!memcmp(&status,&cancelled_status,sizeof(status)));
            CHECK(!fstatat(retained,index->files[0].name,&after,AT_SYMLINK_NOFOLLOW));CHECK(before.st_ino==after.st_ino&&before.st_dev==after.st_dev);CHECK(!close(retained));
            CHECK(!package_assemble(&assembly,&observed));
            /* A changed pin cannot reuse this package directory, even after a
             * completed installation; existing bytes and identity remain intact. */
            char previous=index->manifest_sha256[0];index->manifest_sha256[0]=previous=='a'?'b':'a';
            CHECK(!strcmp(package_assemble(&assembly,&observed),"package_assembly_identity_conflict"));index->manifest_sha256[0]=previous;
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
            memset(&status,0xa5,sizeof(status));PackageAssemblyStatus unchanged=status;CHECK(package_assembly_status(&assembly,&status));CHECK(!memcmp(&status,&unchanged,sizeof(status)));
            changed=openat(payload,index->files[0].name,O_RDONLY|O_NOFOLLOW);CHECK(changed>=0);char byte;CHECK(pread(changed,&byte,1,0)==1&&byte=='X');CHECK(!close(changed));
            CHECK(!close(payload));CHECK(!close(directory));
            printf("PASS staged native speech publication: %zu files; compatibility, completion, revalidation and cancellation\n",index->count);
        }
#ifdef SAMOSA_PACKAGE_REMOTE_TESTS
        if(argc==7){
            int directory=open(argv[4],O_RDONLY|O_DIRECTORY|O_NOFOLLOW);CHECK(directory>=0);CHECK(!mkdirat(directory,"remote-package",0700));
            int remote_directory=openat(directory,"remote-package",O_RDONLY|O_DIRECTORY|O_NOFOLLOW);CHECK(remote_directory>=0);
            char store[4096],payload_path[4096];CHECK(snprintf(store,sizeof(store),"%s/remote-artifacts",argv[4])<(int)sizeof(store));
            CHECK(snprintf(payload_path,sizeof(payload_path),"%s/remote-package/payload",argv[4])<(int)sizeof(payload_path));
            RemoteArtifactSource *downloads=calloc(index->count,sizeof(*downloads));char (*urls)[4096]=calloc(index->count,sizeof(*urls));CHECK(downloads&&urls);
            for(size_t i=0;i<index->count;i++){CHECK(snprintf(urls[i],sizeof(urls[i]),"%s/%s",argv[5],index->files[i].sha256)<(int)sizeof(urls[i]));downloads[i]=(RemoteArtifactSource){.url=urls[i],.ca_file=argv[6],.timeout_ms=5000};}
            PackageRemoteCacheSource remote={.store={.state_path=store,.byte_ceiling=index->payload_bytes,.disk_ceiling=index->payload_bytes*2,.artifact_capacity=PACKAGE_INDEX_FILES,.resume=1},.index=index,.files=downloads,.count=index->count};
            int held=-7;downloads[0].ca_file=NULL;
            CHECK(!strcmp(package_remote_cache_open(&remote,&index->files[0],NULL,NULL,&held),"artifact_tls_verification_failed"));CHECK(held==-7);downloads[0].ca_file=argv[6];
            RemoteArtifactSource digest_download={.url="https://example.invalid/files?redirect=1",.ca_file=argv[6],.timeout_ms=5000};
            remote.digest_source=&digest_download;CHECK(!strcmp(package_remote_cache_open(&remote,&index->files[0],NULL,NULL,&held),"invalid_package_remote_source"));CHECK(held==-7);
            remote.files=NULL;CHECK(!strcmp(package_remote_cache_open(&remote,&index->files[0],NULL,NULL,&held),"invalid_package_remote_source"));CHECK(held==-7);digest_download.url=argv[5];
            CHECK(!mkdirat(directory,"reservation-pool",0700));int reservation_pool=openat(directory,"reservation-pool",O_RDONLY|O_DIRECTORY|O_NOFOLLOW);CHECK(reservation_pool>=0);
            PackageAssembly assembly={.directory=remote_directory,.source=-1,.source_open=package_remote_cache_open,.source_context=&remote,.index=index,.payload_path=payload_path,.copy_ceiling=index->payload_bytes,.reservation_directory=reservation_pool,.reservation_ceiling=index->payload_bytes*2,.reservation_capacity=2};
            PackageTreeResult observed;
            assembly.control=reservation_interrupt;assembly.context=&reservation_pool;
            CHECK(!strcmp(package_assemble(&assembly,&observed),"deadline_exceeded"));
            CHECK(faccessat(remote_directory,"payload",F_OK,0)<0);CHECK(faccessat(reservation_pool,".reservation.pending",F_OK,0)<0);
            PackageAssemblyStatus incomplete;CHECK(!package_assembly_status(&assembly,&incomplete));CHECK(!strcmp(incomplete.state,"incomplete_unverified"));CHECK(!incomplete.package.files);
            assembly.control=NULL;assembly.context=NULL;
            CHECK(!package_assemble(&assembly,&observed));CHECK(observed.files==index->count);
            remote.digest_source=NULL;CHECK(!package_assemble(&assembly,&observed));
            CHECK(!package_remote_cache_open(&remote,&index->files[0],NULL,NULL,&held));CHECK(!close(held));
            /* Cold destination installation uses only retained verified cache
             * descriptors after all URL/credential configuration is removed. */
            CHECK(!mkdirat(directory,"remote-offline-package",0700));int offline_directory=openat(directory,"remote-offline-package",O_RDONLY|O_DIRECTORY|O_NOFOLLOW);CHECK(offline_directory>=0);
            char offline_path[4096];CHECK(snprintf(offline_path,sizeof(offline_path),"%s/remote-offline-package/payload",argv[4])<(int)sizeof(offline_path));
            assembly.directory=offline_directory;assembly.payload_path=offline_path;
            CHECK(!package_assemble(&assembly,&observed));CHECK(observed.files==index->count);CHECK(!close(offline_directory));
            /* The third destination refuses before allocating a payload even
             * though both prior copies remain independently verified/usable. */
            CHECK(!mkdirat(directory,"remote-over-quota",0700));int refused=openat(directory,"remote-over-quota",O_RDONLY|O_DIRECTORY|O_NOFOLLOW);CHECK(refused>=0);
            char refused_path[4096];CHECK(snprintf(refused_path,sizeof(refused_path),"%s/remote-over-quota/payload",argv[4])<(int)sizeof(refused_path));
            assembly.directory=refused;assembly.payload_path=refused_path;
            CHECK(!strcmp(package_assemble(&assembly,&observed),"package_reservation_quota"));CHECK(faccessat(refused,"payload",F_OK,0)<0);CHECK(!close(refused));
            /* Drive the complete installer from the trusted catalog entry;
             * its index is independently acquired and verified in the cache. */
            CHECK(!mkdirat(directory,"catalog-package",0700));int catalog_directory=openat(directory,"catalog-package",O_RDONLY|O_DIRECTORY|O_NOFOLLOW);CHECK(catalog_directory>=0);
            char catalog_path[4096];CHECK(snprintf(catalog_path,sizeof(catalog_path),"%s/catalog-package/payload",argv[4])<(int)sizeof(catalog_path));
            ProviderEntry catalog_entry=entry;CHECK(snprintf(catalog_entry.url,sizeof(catalog_entry.url),"%s/%s",argv[5],index->manifest_sha256)<(int)sizeof(catalog_entry.url));
            CHECK(!mkdirat(directory,"catalog-reservations",0700));int catalog_reservations=openat(directory,"catalog-reservations",O_RDONLY|O_DIRECTORY|O_NOFOLLOW);CHECK(catalog_reservations>=0);
            RemoteArtifactSource index_download={.ca_file=argv[6],.timeout_ms=5000};
            PackageInstaller installer={.entry=&catalog_entry,.store=remote.store,.index_source=&index_download,.file_sources=downloads,.file_count=index->count,
                .assembly={.directory=catalog_directory,.payload_path=catalog_path,.copy_ceiling=index->payload_bytes,.reservation_directory=catalog_reservations,.reservation_ceiling=index->payload_bytes,.reservation_capacity=1}};
            installer.store.disk_ceiling=2*(index->payload_bytes+entry.size);installer.store.artifact_capacity=PACKAGE_INDEX_FILES+1;
            memset(&observed,0xa5,sizeof(observed));PackageTreeResult unchanged_install=observed;
            char supported_recipe[129];strcpy(supported_recipe,catalog_entry.recipe);strcpy(catalog_entry.recipe,"unimplemented-package-v1");
            CHECK(!strcmp(package_install_catalog(&installer,&observed),"unsupported_package_recipe"));CHECK(!memcmp(&observed,&unchanged_install,sizeof(observed)));CHECK(faccessat(catalog_directory,"assemble.lock",F_OK,0)<0);strcpy(catalog_entry.recipe,supported_recipe);
            installer.assembly.reservation_ceiling=0;CHECK(!strcmp(package_install_catalog(&installer,&observed),"invalid_package_installer"));CHECK(!memcmp(&observed,&unchanged_install,sizeof(observed)));installer.assembly.reservation_ceiling=index->payload_bytes;
            char saved_platform[65];strcpy(saved_platform,catalog_entry.platform);strcpy(catalog_entry.platform,!strcmp(saved_platform,"linux-x86_64")?"macos-arm64":"linux-x86_64");
            CHECK(!strcmp(package_install_catalog(&installer,&observed),"unsupported_provider_platform"));CHECK(!memcmp(&observed,&unchanged_install,sizeof(observed)));strcpy(catalog_entry.platform,saved_platform);
            char catalog_json[4096];CHECK(snprintf(catalog_json,sizeof(catalog_json),"{\"catalog_version\":1,\"revision\":\"fixture-v1\",\"entries\":[{\"id\":\"speech-fixture\",\"version\":\"1\",\"url\":\"%s\",\"license\":\"https://example.invalid/license\",\"platform\":\"%s\",\"minimum_os\":\"%s\",\"runtime\":\"system-runtime-v1\",\"recipe\":\"%s\",\"sha256\":\"%s\",\"size\":%llu}]}",catalog_entry.url,catalog_entry.platform,catalog_entry.minimum_os,catalog_entry.recipe,catalog_entry.sha256,(unsigned long long)catalog_entry.size)<(int)sizeof(catalog_json));
            ProviderCatalog *catalog=calloc(1,sizeof(*catalog));CHECK(catalog);CHECK(!provider_catalog_parse(catalog_json,strlen(catalog_json),catalog));
            CHECK(!strcmp(package_install_selected(catalog,"missing",&installer,&observed),"unknown_provider"));CHECK(!memcmp(&observed,&unchanged_install,sizeof(observed)));
            char trusted_catalog[4096];CHECK(snprintf(trusted_catalog,sizeof(trusted_catalog),"%s/catalog.json",argv[4])<(int)sizeof(trusted_catalog));
            int catalog_file=open(trusted_catalog,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);CHECK(catalog_file>=0);CHECK(write(catalog_file,catalog_json,strlen(catalog_json))==(ssize_t)strlen(catalog_json));CHECK(!close(catalog_file));
            CHECK(!chmod(trusted_catalog,0644));CHECK(!strcmp(package_install_from_file(trusted_catalog,"speech-fixture",&installer,&observed),"invalid_catalog_file"));CHECK(!memcmp(&observed,&unchanged_install,sizeof(observed)));CHECK(!chmod(trusted_catalog,0600));
            char catalog_alias[4096];CHECK(snprintf(catalog_alias,sizeof(catalog_alias),"%s/catalog-alias",argv[4])<(int)sizeof(catalog_alias));CHECK(!link(trusted_catalog,catalog_alias));
            CHECK(!strcmp(package_install_from_file(trusted_catalog,"speech-fixture",&installer,&observed),"invalid_catalog_file"));CHECK(!unlink(catalog_alias));
            CHECK(!symlink(trusted_catalog,catalog_alias));CHECK(!strcmp(package_install_from_file(catalog_alias,"speech-fixture",&installer,&observed),"invalid_catalog_file"));CHECK(!unlink(catalog_alias));
            CHECK(!package_install_from_file(trusted_catalog,"speech-fixture",&installer,&observed));CHECK(observed.files==index->count);CHECK(!strcmp(observed.manifest_sha256,index->manifest_sha256));
            installer.index_source=NULL;installer.file_sources=NULL;installer.entry=NULL;CHECK(!package_install_from_file(trusted_catalog,"speech-fixture",&installer,&observed));
            /* Observation needs neither download policy nor a reservation pool. */
            PackageInstaller status_config=installer;status_config.assembly.reservation_directory=-1;status_config.assembly.reservation_ceiling=0;status_config.assembly.reservation_capacity=0;status_config.assembly.copy_ceiling=0;
            PackageAssemblyStatus catalog_status;memset(&catalog_status,0xa5,sizeof(catalog_status));PackageAssemblyStatus status_before=catalog_status;
            CHECK(!strcmp(package_status_from_file(trusted_catalog,"missing",&status_config,&catalog_status),"unknown_provider"));CHECK(!memcmp(&catalog_status,&status_before,sizeof(catalog_status)));
            status_config.assembly.control=cancel;CHECK(!strcmp(package_status_from_file(trusted_catalog,"speech-fixture",&status_config,&catalog_status),"cancelled"));CHECK(!memcmp(&catalog_status,&status_before,sizeof(catalog_status)));status_config.assembly.control=NULL;
            CHECK(!package_status_from_file(trusted_catalog,"speech-fixture",&status_config,&catalog_status));CHECK(!strcmp(catalog_status.state,"installed_verified"));CHECK(catalog_status.package.files==index->count);CHECK(!strcmp(catalog_status.package.manifest_sha256,index->manifest_sha256));
            /* Leave an exact empty-package charge for the public setup gate. */
            CHECK(!mkdirat(directory,"empty-package",0700));CHECK(!mkdirat(directory,"empty-reservations",0700));
            int empty_package=openat(directory,"empty-package",O_RDONLY|O_DIRECTORY|O_NOFOLLOW),empty_pool=openat(directory,"empty-reservations",O_RDONLY|O_DIRECTORY|O_NOFOLLOW);CHECK(empty_package>=0&&empty_pool>=0);
            PackageReservationResult empty_charge;CHECK(!package_reserve(empty_pool,empty_package,index,index->payload_bytes,1,&empty_charge));CHECK(!close(empty_package));CHECK(!close(empty_pool));
            free(catalog);CHECK(!close(catalog_directory));CHECK(!close(catalog_reservations));
            CHECK(!close(reservation_pool));CHECK(!close(remote_directory));CHECK(!close(directory));free(downloads);free(urls);
            printf("PASS complete HTTPS native package: %zu indexed files; untrusted TLS refusal, pinned acquisition, compatibility, publication and credential-free cache reuse\n",index->count);
        }
#else
        CHECK(argc!=7);
#endif
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

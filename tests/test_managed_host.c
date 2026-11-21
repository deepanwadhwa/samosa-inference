#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "managed_host.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
int main(int argc,char **argv) {
    int guardian=engine_guardian_dispatch(argc,argv);if(guardian>=0)return guardian;
    CHECK(argc==1);
    char root[]="/tmp/samosa-managed-host-XXXXXX";CHECK(mkdtemp(root));
    char config[512],embedding[512],state[512],pin[65];memset(pin,'0',64);pin[64]=0;
    snprintf(config,sizeof(config),"%s/config",root);snprintf(embedding,sizeof(embedding),"%s/embedding",root);
    FILE *file=fopen(embedding,"w");CHECK(file&&!fchmod(fileno(file),0600));
    fprintf(file,"{\"model_id\":\"fixture\",\"model_revision\":\"pinned\",\"model_sha256\":\"%s\",\"engine_sha256\":\"%s\",\"tokenizer_hash\":\"%s\",\"query_prefix\":\"query: \",\"document_prefix\":\"\",\"recipe\":\"mean-l2-v1\",\"dimensions\":2}",pin,pin,pin);CHECK(!fclose(file));
    for(int mode=0;mode<2;mode++) {
        snprintf(state,sizeof(state),"%s/state-%d",root,mode);
        file=fopen(config,"w");CHECK(file&&!fchmod(fileno(file),0600));
        fprintf(file,"{\"contract_version\":1,\"profile\":\"%s\",\"state\":\"%s\",\"engine\":\"/missing-owned-engine\",\"engine_sha256\":\"%s\",\"engine_size\":64,\"model\":\"/missing-owned-model\",\"model_sha256\":\"%s\",\"model_size\":3,\"context_tokens\":128,\"model_alias\":\"fixture\",\"owner\":\"alice\",\"port\":53555,\"expected_revision\":0,\"gpu_layers\":0,\"threads\":1",mode?"llama-embedding-static-system-v1":"llama-static-system-v1",state,pin,pin);
        if(mode)fprintf(file,",\"embedding_config_file\":\"%s\"",embedding);
        fputs("}",file);CHECK(!fclose(file));
        ManagedHost host;CHECK(!managed_host_load(config,&host));
        CHECK(host.prepared&&!host.attempted&&host.key_root==-1&&host.key_fd==-1);
        CHECK(host.embedding_mode==mode&&!strcmp(host.owner,"alice")&&host.context==128);
        CHECK(host.key[0]==0&&host.args==NULL);
        CHECK(access(state,F_OK)==-1&&errno==ENOENT);
        /* Discovery owns its parsed snapshot; changing the config afterwards
         * must neither rebind it nor create state during a failed reload. */
        CHECK(!chmod(config,0644));
        ManagedHost refused;
        CHECK(!strcmp(managed_host_load(config,&refused),"invalid_managed_configuration"));
        CHECK(!refused.prepared&&!refused.attempted);
        CHECK(!managed_host_close(&refused));
        CHECK(host.prepared&&!strcmp(host.owner,"alice")&&host.embedding_mode==mode);
        CHECK(access(state,F_OK)==-1&&errno==ENOENT);
        CHECK(!chmod(config,0600));
        char link[512];snprintf(link,sizeof(link),"%s/config-link",root);
        CHECK(!symlink(config,link));
        CHECK(!strcmp(managed_host_load(link,&refused),"invalid_managed_configuration"));
        CHECK(!managed_host_close(&refused));CHECK(!unlink(link));
        CHECK(!managed_host_close(&host));CHECK(!managed_host_close(&host));
        CHECK(managed_host_activate(argv[0],0,NULL,NULL,&host));
        CHECK(access(state,F_OK)==-1&&errno==ENOENT);
        CHECK(!managed_host_load(config,&host));
        CHECK(managed_host_activate(argv[0],0,NULL,NULL,&host));
        CHECK(host.prepared&&!host.attempted);
        CHECK(!managed_host_deactivate(&host));
        CHECK(host.prepared&&!host.args&&!host.key[0]);
        DIR *directory=opendir(state);CHECK(directory);struct dirent *entry;
        while((entry=readdir(directory)))CHECK(strncmp(entry->d_name,"key.",4));
        CHECK(!closedir(directory));CHECK(!managed_host_close(&host));
    }
    char command[768];snprintf(command,sizeof(command),"rm -rf '%s'",root);CHECK(system(command)==0);
    printf("PASS managed host: %u checks; configuration-only text/embedding load, no engine/state work, explicit activation and cleanup\n",checks);
    return 0;
}

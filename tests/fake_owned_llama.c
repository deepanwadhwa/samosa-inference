#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "llama_http_fixture.h"
int main(int argc,char **argv) {
    if(argc!=7||strcmp(argv[1],"--fixture"))return 2;
    char pidfile[1024];snprintf(pidfile,sizeof(pidfile),"%s.pid",argv[4]);
    FILE *file=fopen(pidfile,"w");if(!file)return 2;
    fprintf(file,"%ld\n",(long)getpid());if(fclose(file))return 2;
    /* Exercise the guardian's bound model descriptor, not a private model path. */
    file=fopen(argv[6],"r");if(!file)return 2;
    char bytes[4]={0};size_t n=fread(bytes,1,3,file);fclose(file);
    if(n!=3||strcmp(bytes,"abc"))return 2;
    return llama_fixture_server(argv[2],argv[3],argv[4],(unsigned)strtoul(argv[5],NULL,10),"fixture-secret");
}

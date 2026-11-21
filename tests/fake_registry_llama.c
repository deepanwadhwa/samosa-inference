#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "llama_http_fixture.h"
int main(int argc,char **argv){
    const char *key_path=NULL,*model=NULL;unsigned port=0;
    for(int i=1;i+1<argc;i++){
        if(!strcmp(argv[i],"--api-key-file"))key_path=argv[++i];
        else if(!strcmp(argv[i],"--model"))model=argv[++i];
        else if(!strcmp(argv[i],"--port"))port=(unsigned)strtoul(argv[++i],NULL,10);
    }
    if(!key_path||!model||!port)return 2;
    char root[4096],key[65]={0},mode[64]="good",path[4096],log[4096],portfile[4096];
    snprintf(root,sizeof(root),"%s",key_path);char *slash=strrchr(root,'/');if(!slash)return 2;*slash=0;
    FILE *f=fopen(key_path,"r");if(!f)return 2;size_t n=fread(key,1,64,f);fclose(f);if(n!=64)return 2;
    f=fopen(model,"r");if(!f)return 2;char content[4]={0};n=fread(content,1,3,f);fclose(f);if(n!=3||strcmp(content,"abc"))return 2;
    if(snprintf(path,sizeof(path),"%s/mode",root)>=(int)sizeof(path))return 2;
    f=fopen(path,"r");if(f){if(fscanf(f,"%63s",mode)!=1)return 2;fclose(f);}
    if(snprintf(log,sizeof(log),"%s/fixture.log",root)>=(int)sizeof(log)||
       snprintf(portfile,sizeof(portfile),"%s/fixture.port",root)>=(int)sizeof(portfile)||
       snprintf(path,sizeof(path),"%s/fixture.pid",root)>=(int)sizeof(path))return 2;
    f=fopen(path,"w");if(!f)return 2;fprintf(f,"%ld\n",(long)getpid());if(fclose(f))return 2;
    return llama_fixture_server(portfile,mode,log,port,key);
}

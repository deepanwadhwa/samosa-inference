#define _POSIX_C_SOURCE 200809L
#include "catalog.h"
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do { checks++; if(!(x)){fprintf(stderr,"catalog failure line %d\n",__LINE__);exit(1);} } while(0)
static const char entry[]="{\"id\":\"fixture\",\"version\":\"1\",\"url\":\"https://example.invalid/engine\",\"license\":\"Fixture license notice\",\"platform\":\"macos-arm64\",\"minimum_os\":\"26.0\",\"runtime\":\"system-runtime-v1\",\"recipe\":\"static-1\",\"sha256\":\"1634265c6be962401fb2d2419b371b5eedef20957bc603fb3bbde62ed8389ccf\",\"size\":20513984}";
static void replace(char *out,size_t capacity,const char *text,const char *needle,const char *value) {
    const char *found=strstr(text,needle);CHECK(found);
    int n=snprintf(out,capacity,"%.*s%s%s",(int)(found-text),text,value,found+strlen(needle));CHECK(n>0&&(size_t)n<capacity);
}
int main(void) {
    char input[PROVIDER_CATALOG_BYTES+1];ProviderCatalog c;const ProviderEntry *e=NULL;
    snprintf(input,sizeof(input),"{\"catalog_version\":1,\"revision\":\"test-1\",\"entries\":[%s]}",entry);
    CHECK(!provider_catalog_parse(input,strlen(input),&c));CHECK(c.count==1);
    char path[]="/tmp/samosa-catalog.XXXXXX";int fd=mkstemp(path);CHECK(fd>=0);
    CHECK(write(fd,input,strlen(input))==(ssize_t)strlen(input));CHECK(!close(fd));
    CHECK(!provider_catalog_load(path,&c));
    CHECK(!chmod(path,0644));CHECK(provider_catalog_load(path,&c));CHECK(!chmod(path,0600));
    char alias[256];snprintf(alias,sizeof(alias),"%s-link",path);
    CHECK(!symlink(path,alias));CHECK(provider_catalog_load(alias,&c));CHECK(!unlink(alias));
    CHECK(!link(path,alias));CHECK(provider_catalog_load(path,&c));CHECK(!unlink(alias));
    CHECK(provider_catalog_load("relative",&c));CHECK(!unlink(path));CHECK(provider_catalog_load(path,&c));
    CHECK(!provider_catalog_select(&c,"fixture","macos-arm64",&e));CHECK(e&&e->size==20513984);
    ArtifactConfig config={0};provider_catalog_artifact(e,&config);CHECK(config.size==e->size);CHECK(!strcmp(config.sha256,e->sha256));
    CHECK(provider_catalog_select(&c,"fixture","linux-arm64",&e));CHECK(provider_catalog_select(&c,"missing","macos-arm64",&e));
    strcpy(c.entries[0].runtime,"unsupported");CHECK(provider_catalog_select(&c,"fixture","macos-arm64",&e));
    ProviderCatalog before=c;
    for(size_t i=0;i<strlen(input);i++){CHECK(provider_catalog_parse(input,i,&c));CHECK(!memcmp(&before,&c,sizeof(c)));}
    CHECK(provider_catalog_parse(input,PROVIDER_CATALOG_BYTES+1,&c));
    snprintf(input,sizeof(input),"{\"catalog_version\":1,\"revision\":\"test\",\"entries\":[%s,%s]}",entry,entry);CHECK(provider_catalog_parse(input,strlen(input),&c));
    const char *bad[]={"{}","{\"catalog_version\":2,\"revision\":\"r\",\"entries\":[]}","{\"catalog_version\":1,\"revision\":\"r\",\"entries\":[],\"url\":\"override\"}","null"};
    for(size_t i=0;i<sizeof(bad)/sizeof(*bad);i++)CHECK(provider_catalog_parse(bad[i],strlen(bad[i]),&c));
    const char empty[]="{\"catalog_version\":1,\"revision\":\"r\",\"entries\":[]}";
    CHECK(!provider_catalog_parse(empty,strlen(empty),&c));CHECK(!c.count);
    uint32_t parts[3];CHECK(provider_os_version("26.1",parts));CHECK(parts[0]==26&&parts[1]==1&&parts[2]==0);
    const char *versions[]={"","26.",".26","26..1","26.1.2.3","65536","-1","1x","1.2-foo"};
    for(size_t i=0;i<sizeof(versions)/sizeof(*versions);i++)CHECK(!provider_os_version(versions[i],parts));
    ProviderEntry os_entry={0};strcpy(os_entry.minimum_os,"26.1.2");
    CHECK(provider_catalog_qualify(&os_entry,"26.1.1"));CHECK(!provider_catalog_qualify(&os_entry,"26.1.2"));
    CHECK(!provider_catalog_qualify(&os_entry,"26.2"));CHECK(!provider_catalog_qualify(&os_entry,"27"));CHECK(provider_catalog_qualify(&os_entry,"25.99.99"));
    char platform[65],os[65];CHECK(!provider_catalog_host(platform,os));CHECK(provider_os_version(os,parts));
#if defined(__APPLE__) && defined(__x86_64__)
    CHECK(!strcmp(platform,"macos-x86_64"));
#elif defined(__APPLE__)
    CHECK(!strcmp(platform,"macos-arm64"));
#elif defined(__linux__) && defined(__x86_64__)
    CHECK(!strcmp(platform,"linux-x86_64"));
#elif defined(__linux__)
    CHECK(!strcmp(platform,"linux-arm64"));
#endif
    const char *needles[]={"https://example.invalid/engine","26.0","20513984","system-runtime-v1","Fixture license notice","fixture"};
    const char *invalid[][6]={
        {"http://example.invalid/engine","file:///engine","https://user@example.invalid/engine","https://example.invalid/engine#fragment","https://example.invalid/a b","https://example.invalid/a\\n"},
        {"26.0.0.0","latest","26.","65536","26..1","26.0-foo"},
        {"0","-1","9223372036854775808","18446744073709551616","1.5","null"},
        {"","a b","a\\n","a/b","a:b","a@b"},
        {"","a\\n","a\\r","a\\t","a\\u0000","a\\u007f"},
        {"","a b","a/b","a\\n","a@b","a:b"}
    };
    char changed[8192];
    for(size_t i=0;i<6;i++)for(size_t k=0;k<6;k++) {
        replace(changed,sizeof(changed),entry,needles[i],invalid[i][k]);
        snprintf(input,sizeof(input),"{\"catalog_version\":1,\"revision\":\"r\",\"entries\":[%s]}",changed);
        CHECK(provider_catalog_parse(input,strlen(input),&c));
    }
    size_t used=(size_t)snprintf(input,sizeof(input),"{\"catalog_version\":1,\"revision\":\"r\",\"entries\":[");
    for(unsigned i=0;i<17;i++) {
        char id[32];snprintf(id,sizeof(id),"fixture-%u",i);replace(changed,sizeof(changed),entry,"fixture",id);
        int n=snprintf(input+used,sizeof(input)-used,"%s%s",i?",":"",changed);CHECK(n>0);used+=(size_t)n;
        char bounded[PROVIDER_CATALOG_BYTES+1];CHECK(used+3<sizeof(bounded));memcpy(bounded,input,used);memcpy(bounded+used,"]}",3);
        if(i<16){CHECK(!provider_catalog_parse(bounded,used+2,&c));CHECK(c.count==i+1);}
        else CHECK(provider_catalog_parse(bounded,used+2,&c));
    }
    printf("PASS provider catalog: %u checks\n",checks);return 0;
}

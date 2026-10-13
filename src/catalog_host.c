#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "catalog.h"
#include <sys/utsname.h>
#include <string.h>
#include <stdint.h>
#ifdef __APPLE__
#include <sys/sysctl.h>
#endif
int provider_os_version(const char *text,uint32_t parts[3]) {
    if(!text||!*text)return 0;
    uint32_t parsed[3]={0};unsigned i=0;
    while(*text) {
        if(i==3||*text<'0'||*text>'9')return 0;
        uint32_t value=0;
        do {unsigned digit=(unsigned)(*text++-'0');if(value>(65535-digit)/10)return 0;value=value*10+digit;}while(*text>='0'&&*text<='9');
        parsed[i++]=value;
        if(!*text)break;
        if(*text++!='.'||!*text)return 0;
    }
    memcpy(parts,parsed,sizeof(parsed));return 1;
}
const char *provider_catalog_qualify(const ProviderEntry *entry,const char *host_version) {
    uint32_t required[3],host[3];
    if(!entry||!provider_os_version(entry->minimum_os,required)||!provider_os_version(host_version,host))return "invalid_provider_os_version";
    for(unsigned i=0;i<3;i++) {
        if(host[i]>required[i])return NULL;
        if(host[i]<required[i])return "unsupported_provider_os";
    }
    return NULL;
}
const char *provider_catalog_host(char platform[65],char os[65]) {
#if defined(__APPLE__) && (defined(__arm64__) || defined(__aarch64__))
    size_t n=65;
    if(sysctlbyname("kern.osproductversion",os,&n,NULL,0)||!n||n>65||os[n-1])return "unsupported_provider_host";
    strcpy(platform,"macos-arm64");
#elif defined(__linux__) && defined(__aarch64__)
    struct utsname u;if(uname(&u))return "unsupported_provider_host";
    size_t n=0;while(u.release[n]&&((u.release[n]>='0'&&u.release[n]<='9')||u.release[n]=='.'))n++;
    if(!n||n>=65)return "unsupported_provider_host";
    memcpy(os,u.release,n);os[n]=0;strcpy(platform,"linux-arm64");
#else
    (void)platform;(void)os;return "unsupported_provider_host";
#endif
    uint32_t parts[3];return provider_os_version(os,parts)?NULL:"unsupported_provider_host";
}

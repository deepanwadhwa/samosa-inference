#include "package_index.h"
#include "catalog.h"
#include "platform.h"
#include "sha256.h"
#define JSON_TOKENS_MAX 8192
#include "json.h"
static int pin(const char *s){return s&&strlen(s)==64&&strspn(s,"0123456789abcdef")==64;}
static int field(const Json *j,int object,const char *name,char *out,size_t capacity){
    char *s=json_string(j,json_get(j,object,name));int ok=s&&*s&&strlen(s)<capacity;
    if(ok)memcpy(out,s,strlen(s)+1);free(s);return ok;
}
static int identifier(const char *s){
    for(;*s;s++)if(!((*s>='a'&&*s<='z')||(*s>='A'&&*s<='Z')||(*s>='0'&&*s<='9')||strchr("._-",*s)))return 0;
    return 1;
}
static int relative(const char *name,enum PackageFileRole role){
    unsigned components=0;
    for(const char *p=name;*p;){
        const char *begin=p;if(++components>8||*p=='.')return 0;
        if(role==PACKAGE_DATA&&!strncmp(p,"!v",2)&&(!p[2]||p[2]=='/'))p+=2;
        else if(role==PACKAGE_DATA&&p-name>=3&&!memcmp(p-3,"!v/",3)&&!strncmp(p,"Mr serious",10)&&(!p[10]||p[10]=='/'))p+=10;
        else while(*p&&*p!='/'){
            unsigned char c=(unsigned char)*p++;
            if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||strchr("-_.",c)))return 0;
        }
        if(p==begin)return 0;if(*p&&!*++p)return 0;
    }
    return components!=0;
}
static enum PackageFileRole role(const char *s){
    static const char *names[]={"engine","library","model","data","metadata"};
    for(size_t i=0;i<sizeof(names)/sizeof(*names);i++)if(!strcmp(s,names[i]))return (enum PackageFileRole)(i+1);
    return 0;
}
static int conflicts(const char *a,const char *b){
    size_t an=strlen(a),bn=strlen(b);
    /* Conservative portable names also avoid aliases on case-insensitive
     * macOS filesystems before any installation directory is created. */
    size_t n=an<bn?an:bn;
    for(size_t i=0;i<n;i++){
        unsigned char ac=(unsigned char)a[i],bc=(unsigned char)b[i];
        if(ac>='A'&&ac<='Z')ac=(unsigned char)(ac-'A'+'a');
        if(bc>='A'&&bc<='Z')bc=(unsigned char)(bc-'A'+'a');
        if(ac!=bc)return 0;
    }
    return an==bn||(an<bn&&b[an]=='/')||(bn<an&&a[bn]=='/');
}
const char *package_index_parse(const char *bytes,size_t length,const char *expected,PackageIndex *out){
    if(!bytes||!out||!pin(expected)||!length||length>PACKAGE_INDEX_BYTES)return "invalid_package_index";
    char actual[65];hash_bytes(bytes,length,actual);
    if(strcmp(actual,expected))return "package_index_checksum";
    PackageIndex *next=calloc(1,sizeof(*next));Json *j=malloc(sizeof(*j));
    if(!next||!j){free(next);free(j);return "allocation_failed";}
    const char *e="invalid_package_index";uint32_t version[3];char format[65];
    if(!json_parse(j,bytes,length)||j->tokens[0].type!=JT_OBJECT||j->tokens[0].count!=5||
       !field(j,0,"format",format,sizeof(format))||strcmp(format,"samosa.inference.package.v1")||
       !field(j,0,"platform",next->platform,sizeof(next->platform))||!inference_platform_valid(next->platform)||
       !field(j,0,"minimum_os",next->minimum_os,sizeof(next->minimum_os))||!provider_os_version(next->minimum_os,version)||
       !field(j,0,"recipe",next->recipe,sizeof(next->recipe))||!identifier(next->recipe))goto done;
    int files=json_get(j,0,"files");
    if(files<0||j->tokens[files].type!=JT_ARRAY||!j->tokens[files].count||(size_t)j->tokens[files].count>PACKAGE_INDEX_FILES)goto done;
    for(int t=files+1;t<j->tokens[files].next;t=j->tokens[t].next){
        PackageFile *file=&next->files[next->count];char selected[32];size_t size;
        if(j->tokens[t].type!=JT_OBJECT||j->tokens[t].count!=4||
           !field(j,t,"name",file->name,sizeof(file->name))||!field(j,t,"role",selected,sizeof(selected))||
           !(file->role=role(selected))||!relative(file->name,file->role)||
           !field(j,t,"sha256",file->sha256,sizeof(file->sha256))||!pin(file->sha256)||
           !json_uint(j,json_get(j,t,"bytes"),&size)||!size||size>PACKAGE_PAYLOAD_BYTES-next->payload_bytes)goto done;
        for(size_t i=0;i<next->count;i++)if(conflicts(file->name,next->files[i].name)||
            (!strcmp(file->sha256,next->files[i].sha256)&&size!=next->files[i].bytes))goto done;
        file->bytes=size;next->payload_bytes+=size;next->count++;
    }
    strcpy(next->manifest_sha256,actual);*out=*next;e=NULL;
done:free(j);free(next);return e;
}
const char *package_index_qualify(const PackageIndex *index,const char *platform,const char *os){
    if(!index||!platform||!os||!index->count||index->count>PACKAGE_INDEX_FILES||
        !memchr(index->platform,0,sizeof(index->platform))||!memchr(index->minimum_os,0,sizeof(index->minimum_os))||!inference_platform_valid(platform)||
        !inference_platform_valid(index->platform))return "invalid_package_index";
    if(strcmp(index->platform,platform))return "unsupported_provider_platform";
    ProviderEntry entry={0};memcpy(entry.minimum_os,index->minimum_os,sizeof(entry.minimum_os));
    return provider_catalog_qualify(&entry,os);
}

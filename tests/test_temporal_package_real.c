#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include "whisper_fixture.h"
#include "temporal_package.h"
static const char *cancelled(void *context){(void)context;return "cancelled";}
int main(int argc,char **argv){
    CHECK(argc==2&&argv[1][0]=='/');char manifest[4096],pin[65],identity[65];CHECK(snprintf(manifest,sizeof(manifest),"%s/manifest.json",argv[1])<(int)sizeof(manifest));file_sha(manifest,pin);temporal_package_identity(pin,identity);CHECK(strlen(identity)==64);
    int directory=open(argv[1],O_RDONLY|O_DIRECTORY|O_NOFOLLOW);CHECK(directory>=0);TemporalPackageReceipt *receipt=calloc(1,sizeof(*receipt));CHECK(receipt);
    const char *error=temporal_package_verify(directory,argv[1],pin,cancelled,NULL,receipt,0);CHECK(error&&!strcmp(error,"cancelled"));
    char wrong[65];memset(wrong,'0',64);wrong[64]=0;error=temporal_package_verify(directory,argv[1],wrong,NULL,NULL,receipt,0);CHECK(error&&!strcmp(error,"artifact_checksum_mismatch"));
    error=temporal_package_verify(directory,argv[1],pin,NULL,NULL,receipt,0);if(error)fprintf(stderr,"verify: %s\n",error);CHECK(!error&&receipt->count==24&&receipt->total_bytes>3000000000u&&receipt->total_bytes<4294967296ull);
    error=temporal_package_verify(directory,argv[1],pin,NULL,NULL,receipt,1);if(error)fprintf(stderr,"recheck: %s\n",error);CHECK(!error);
    receipt->files[0].identity.st_ino++;error=temporal_package_verify(directory,argv[1],pin,NULL,NULL,receipt,1);CHECK(error&&!strcmp(error,"artifact_changed"));
    CHECK(!close(directory));printf("PASS actual temporal package: %u checks;24 pinned files, pre/post verification, cancellation, wrong manifest pin and changed identity refusal; manifest=%s model=%s bytes=%llu\n",checks,pin,identity,(unsigned long long)receipt->total_bytes);free(receipt);return 0;
}

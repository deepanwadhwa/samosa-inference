#include "decision_package_descriptor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"decision descriptor failure %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void rejected(const char *body,size_t bytes,const PackageIndex *index,const char *expected) {
    DecisionPackageDescriptor output;memset(&output,0xa5,sizeof(output));
    DecisionPackageDescriptor before=output;
    const char *error=decision_package_descriptor_parse(body,bytes,index,&output);
    CHECK(error&&(!expected||!strcmp(error,expected)));CHECK(!memcmp(&before,&output,sizeof(output)));
}
int main(void) {
    PackageIndex *index=calloc(1,sizeof(*index));CHECK(index);index->count=5;
    const size_t positions[]={4,3,0,2};
    const enum PackageFileRole roles[]={PACKAGE_ENGINE,PACKAGE_LIBRARY,PACKAGE_MODEL,PACKAGE_DATA};
    for(unsigned i=0;i<DECISION_ASSET_COUNT;i++) {
        PackageFile *file=&index->files[positions[i]];
        snprintf(file->name,sizeof(file->name),"assets/%s",decision_asset_name(i));file->role=roles[i];
    }
    strcpy(index->files[1].name,"decision-package.json");index->files[1].role=PACKAGE_METADATA;
    const char *body="{\"format\":\"samosa.inference.decision-package.v1\",\"profile\":\"image\",\"protocol\":\"opendecision-framed-v1\",\"assets\":\"assets\",\"runtime_revision\":\"327d6911fc7f31e779d2fa673e83ba1f3410b468\"}";
    DecisionPackageDescriptor output;
    CHECK(!decision_package_descriptor_parse(body,strlen(body),index,&output));
    CHECK(output.profile==DECISION_ASSETS_IMAGE);
    for(unsigned i=0;i<DECISION_ASSET_COUNT;i++)CHECK(output.files[i]==positions[i]);
    const char *text="{\"format\":\"samosa.inference.decision-package.v1\",\"profile\":\"text\",\"protocol\":\"opendecision-framed-v1\",\"assets\":\"assets\",\"runtime_revision\":\"327d6911fc7f31e779d2fa673e83ba1f3410b468\"}";
    CHECK(!decision_package_descriptor_parse(text,strlen(text),index,&output)&&output.profile==DECISION_ASSETS_TEXT);
    const char *invalid[]={"{}",
        "{\"format\":\"samosa.inference.decision-package.v1\",\"profile\":\"unknown\",\"protocol\":\"opendecision-framed-v1\",\"assets\":\"assets\",\"runtime_revision\":\"327d6911fc7f31e779d2fa673e83ba1f3410b468\"}",
        "{\"format\":\"samosa.inference.decision-package.v1\",\"profile\":\"image\",\"command\":\"sh\",\"assets\":\"assets\",\"runtime_revision\":\"327d6911fc7f31e779d2fa673e83ba1f3410b468\"}",
        "{\"format\":\"samosa.inference.decision-package.v1\",\"profile\":\"image\",\"protocol\":\"opendecision-framed-v1\",\"assets\":\"../assets\",\"runtime_revision\":\"327d6911fc7f31e779d2fa673e83ba1f3410b468\"}",
        "{\"format\":\"samosa.inference.decision-package.v1\",\"profile\":\"image\",\"protocol\":\"unknown\",\"assets\":\"assets\",\"runtime_revision\":\"327d6911fc7f31e779d2fa673e83ba1f3410b468\"}",
        "{\"format\":\"samosa.inference.decision-package.v1\",\"profile\":\"image\",\"protocol\":\"opendecision-framed-v1\",\"assets\":\"assets\",\"runtime_revision\":\"bad\"}",
        "{\"format\":\"samosa.inference.decision-package.v1\",\"profile\":\"image\",\"protocol\":\"opendecision-framed-v1\",\"assets\":\"assets\",\"runtime_revision\":\"327d6911fc7f31e779d2fa673e83ba1f3410b468\",\"extra\":0}"};
    for(size_t i=0;i<sizeof(invalid)/sizeof(*invalid);i++)rejected(invalid[i],strlen(invalid[i]),index,"invalid_decision_package_descriptor");
    index->files[positions[DECISION_TOKENIZER]].role=PACKAGE_MODEL;
    rejected(body,strlen(body),index,"decision_package_identity_conflict");index->files[positions[DECISION_TOKENIZER]].role=PACKAGE_DATA;
    index->count=6;strcpy(index->files[5].name,"provenance.json");index->files[5].role=PACKAGE_METADATA;
    CHECK(!decision_package_descriptor_parse(body,strlen(body),index,&output));
    strcpy(index->files[5].name,"assets/provenance.json");rejected(body,strlen(body),index,"decision_package_identity_conflict");
    strcpy(index->files[5].name,"other-engine");index->files[5].role=PACKAGE_ENGINE;rejected(body,strlen(body),index,"decision_package_identity_conflict");
    index->files[5]=index->files[0];rejected(body,strlen(body),index,"decision_package_identity_conflict");
    memset(index->files[5].name,'x',sizeof(index->files[5].name));rejected(body,strlen(body),index,"invalid_package_index");
    index->count=4;rejected(body,strlen(body),index,"invalid_decision_package_descriptor");index->count=5;
    for(size_t n=0;n<strlen(body);n++)rejected(body,n,index,"invalid_decision_package_descriptor");
    rejected(NULL,1,index,"invalid_decision_package_descriptor");rejected(body,4097,index,"invalid_decision_package_descriptor");
    CHECK(decision_package_descriptor_parse(body,strlen(body),index,NULL));
    free(index);printf("PASS decision package descriptor: %u checks; text/image profiles, fixed protocol, exact asset roles, closed subtree and atomic refusal\n",checks);
    return 0;
}

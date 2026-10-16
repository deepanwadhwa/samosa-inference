/* Static actual-asset gate only; no engine or model is executed here. */
#include "decision_assets.h"
#include <stdio.h>
#include <string.h>
int main(int argc,char **argv) {
    if(argc!=6&&(argc!=7||strcmp(argv[6],"--image")))return 2;
    unsigned profile=argc==7?DECISION_ASSETS_IMAGE:DECISION_ASSETS_TEXT;
    const char *pins[]={argv[2],argv[3],argv[4],argv[5]};DecisionAssets assets={0};
    const char *error=decision_assets_open_profile(argv[1],pins,profile,NULL,NULL,&assets);
    if(!error)error=decision_assets_recheck(&assets,NULL,NULL);
    decision_assets_close(&assets);
    if(error){fprintf(stderr,"FAIL decision assets: %s\n",error);return 1;}
    puts("PASS actual decision assets: pinned closed root and host loader graph; model readiness not tested");return 0;
}

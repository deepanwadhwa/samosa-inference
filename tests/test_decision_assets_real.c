/* Static actual-asset gate only; no engine or model is executed here. */
#include "decision_assets.h"
#include <stdio.h>
int main(int argc,char **argv) {
    if(argc!=6)return 2;
    const char *pins[]={argv[2],argv[3],argv[4],argv[5]};DecisionAssets assets={0};
    const char *error=decision_assets_open(argv[1],pins,NULL,NULL,&assets);
    if(!error)error=decision_assets_recheck(&assets,NULL,NULL);
    decision_assets_close(&assets);
    if(error){fprintf(stderr,"FAIL decision assets: %s\n",error);return 1;}
    puts("PASS actual decision assets: pinned closed root and host loader graph; model readiness not tested");return 0;
}

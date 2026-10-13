#include "speech_worker.h"
#include <stdio.h>
int main(int argc,char **argv) {
    int code=engine_guardian_dispatch(argc,argv);if(code>=0)return code;
    code=speech_worker_dispatch(argc,argv);if(code>=0)return code;
    fputs("samosa-inference speech worker requires owned host descriptors\n",stderr);return 2;
}

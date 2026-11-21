#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include <samosa_inference/whisper_process.h>
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Native generated "quick brown fox" recording and retained installed assets.
 * This is bounded recognition evidence, not a general model-quality claim. */
int main(int argc,char **argv) {
    assert(argc==4&&argv[1][0]=='/'&&argv[2][0]=='/'&&argv[3][0]=='/');alarm(180);
    char temporary[]="/tmp/samosa-whisper-process-real-XXXXXX";assert(mkdtemp(temporary));
    SamosaWhisperProcessConfig config={argv[1],argv[2],temporary};
    for(int srt=0;srt<2;srt++) {
        char *text=NULL;int result=samosa_whisper_process_run(&config,NULL,argv[3],srt,0,&text);
        if(result!=SAMOSA_WHISPER_OK||!text)fprintf(stderr,"actual Whisper mode=%d result=%d\n",srt,result);
        assert(result==SAMOSA_WHISPER_OK&&text);
        printf("Actual %s: %s\n",srt?"SRT":"text",text);
        for(char *p=text;*p;p++)*p=(char)tolower((unsigned char)*p);
        assert(strstr(text,"fox")&&strstr(text,"dog"));if(srt)assert(strstr(text," --> "));
        free(text);DIR *d=opendir(temporary);assert(d);struct dirent *entry;
        while((entry=readdir(d)))assert(!strcmp(entry->d_name,".")||!strcmp(entry->d_name,".."));
        assert(!closedir(d));
    }
    assert(!rmdir(temporary));puts("PASS retained actual Mac Whisper: original text/SRT CLI, expected spoken words and output cleanup");return 0;
}

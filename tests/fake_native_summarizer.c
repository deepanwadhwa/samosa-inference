#include <arpa/inet.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int exact_read(void *buffer, size_t length) {
    unsigned char *cursor = buffer;
    while (length) {
        ssize_t count = read(STDIN_FILENO, cursor, length);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return 0;
        cursor += count; length -= (size_t)count;
    }
    return 1;
}

static int exact_write(const void *buffer, size_t length) {
    const unsigned char *cursor = buffer;
    while (length) {
        ssize_t count = write(STDOUT_FILENO, cursor, length);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return 0;
        cursor += count; length -= (size_t)count;
    }
    return 1;
}

int main(int argc,char **argv) {
    const char *mode=getenv("SAMOSA_FAKE_SUMMARIZER_MODE"),*marker=getenv("SAMOSA_FAKE_SUMMARIZER_MARKER"),*test_log=NULL;
    if(!mode)mode="echo";
    char configured[64];const char *mode_file=getenv("SAMOSA_FAKE_SUMMARIZER_CONTROL");
    if(mode_file){FILE *f=fopen(mode_file,"r");if(!f)return 5;if(fscanf(f,"%63s",configured)!=1)return 5;fclose(f);mode=configured;}
    for(int i=1;i+1<argc;i++){
        if(!strcmp(argv[i],"--test-summary-mode"))mode=argv[++i];
        else if(!strcmp(argv[i],"--test-summary-marker"))marker=argv[++i];
        else if(!strcmp(argv[i],"--test-summary-log"))test_log=argv[++i];
    }
    if(!strcmp(mode,"stall-input")){
        if(marker){FILE *f=fopen(marker,"w");if(!f)return 5;fclose(f);}
        for(;;)pause();
    }
    if(!strcmp(mode,"closed-input"))return 0;
    unsigned requests=0;
    for (;;) {
        uint32_t encoded = 0;
        if (!exact_read(&encoded, sizeof(encoded))) return 0;
        uint32_t length = ntohl(encoded);
        unsigned budget=128;
        if(length==0x20000000U){
            if(!strcmp(mode,"legacy"))return 0;
            if(!exact_read(&encoded,sizeof(encoded)))return 2;budget=ntohl(encoded);
            if(budget<16||budget>256||!exact_read(&encoded,sizeof(encoded)))return 2;
            length=ntohl(encoded);
        }
        int count = (length & 0x80000000U) ? (int)(length & 0x3fffffffU) : 1;
        int batch = (length & 0x80000000U) != 0;
        requests++;
        const char *budget_log=getenv("SAMOSA_FAKE_SUMMARIZER_BUDGET_LOG");
        if(budget_log){FILE *f=fopen(budget_log,"a");if(!f)return 5;fprintf(f,"%ld %u %d\n",(long)getpid(),budget,batch);fclose(f);}
        if (count < 1 || count > 64) return 2;
        char *prompts[64] = {0};
        /* Drain the complete request before writing replies, just like the
           native engine. Large batches must not deadlock two full pipes. */
        for (int i = 0; i < count; i++) {
            if(!strcmp(mode,"stall-output")||(!strcmp(mode,"stall-after-warm")&&requests>1)){
                if(marker){FILE *f=fopen(marker,"w");if(!f)return 5;fclose(f);}
                for(;;)pause();
            }
            if (batch) {
                if (!exact_read(&encoded, sizeof(encoded))) return 2;
                length = ntohl(encoded);
            }
            if (!length || length > 65536) return 2;
            prompts[i] = malloc((size_t)length + 1);
            if (!prompts[i] || !exact_read(prompts[i], length)) return 3;
            prompts[i][length] = 0;
        }
        for (int i = 0; i < count; i++) {
            const char *fixed =
                "South Carolina DPH confirms 30 cyclosporiasis cases in 2026 and provides produce-washing and symptom guidance.";
            const char *summary = strstr(prompts[i], "South Carolina") ? fixed : prompts[i];
            if (!strncmp(summary, "summarize: ", 11)) summary += 11;
            size_t output_length = strlen(summary);
            if(!strcmp(mode,"empty"))output_length=0;
            if (output_length > 260) output_length = 260;
            const char *log_path = test_log?test_log:getenv("SAMOSA_FAKE_SUMMARIZER_LOG");
            if (log_path) {
                FILE *log = fopen(log_path, "a");
                if (log) { fprintf(log, "%ld\n", (long)getpid()); fclose(log); }
            }
            uint32_t output_encoded = htonl((uint32_t)output_length);
            if(!strcmp(mode,"invalid-utf8")){summary="\xff invalid reply";output_length=strlen(summary);output_encoded=htonl((uint32_t)output_length);}
            int ok = exact_write(&output_encoded, sizeof(output_encoded)) && exact_write(summary, output_length);
            free(prompts[i]);
            if (!ok) return 4;
        }
    }
}

#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include <samosa_inference/summary_process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "summary_owned_fixture.h"

/* Preserve the original native acceptance corpus through the extracted owner
 * transport: distinct subjects/numbers expose cross-attention/KV contamination. */
int main(int argc, char **argv) {
    int dispatch=engine_guardian_dispatch(argc,argv);if(dispatch>=0)return dispatch;
    int owned=argc==4&&!strcmp(argv[3],"--guardian");
    if (argc != 3&&!owned) return 2;
    SamosaSummaryProcess process;
    if (!samosa_summary_process_init(&process)) return 3;
    char log[] = "/tmp/samosa-summary-transport-real.XXXXXX";
    int fd = mkstemp(log);
    if (fd < 0) { samosa_summary_process_destroy(&process); return 4; }
    close(fd);
    char root[]="/tmp/samosa-summary-real-owned-XXXXXX";
    SummaryOwnedFixture host;
    if(owned){
        if(!mkdtemp(root))return 4;
        owned_init(&host,argv[0],root);SamosaSummaryLifecycle lifecycle={owned_open,owned_close,&host};
        if(!samosa_summary_process_bind(&process,&lifecycle))return 4;
    }
    SamosaSummaryProcessConfig config = {argv[1], argv[2], log, "0"};
    const char *source =
        "South Carolina officials reported 30 cases of cyclosporiasis in 2026. "
        "None were linked to the national outbreak. Eleven cases had been reported by July 8, "
        "but the total increased to 30 by July 24. The illness can cause watery diarrhea.";
    char *summary = samosa_summary_process_once(&process, &config, source, strlen(source));
    int ok = summary && strstr(summary, "30 cases") && !strstr(summary, "29");
    pid_t resident = process.pid;
    int guardian=owned?atomic_load(&host.guardian):0;
    if(owned)ok=ok&&guardian>0&&atomic_load(&host.opens)==1;
    char *sources[] = {
        "South Carolina officials reported 30 cases of cyclosporiasis in 2026. None were linked to the national outbreak.",
        "Cedar Harbor council approved planting 42 oak trees in Riverside Park. Volunteers will plant the trees in November.",
        "The Maple library has 17 new computers. The computers arrived on Tuesday and visitors can use them during opening hours.",
        "The Pine school hired 8 teachers for its new language program. The program begins in September and classes will meet twice weekly."
    };
    const char *subjects[] = {"South Carolina", "Cedar Harbor", "Maple", "Pine"};
    const char *numbers[] = {"30", "42", "17", "8"};
    for (int round = 0; ok && round < 2; round++) {
        char *replies[4] = {NULL, NULL, NULL, NULL};
        ok = (round == 0
              ? samosa_summary_process_batch(&process, &config, sources, 4, replies, 1)
              : samosa_summary_process_coalesce(&process, &config, sources, 4, replies, 1, NULL, NULL))
             && (owned?atomic_load(&host.guardian)==guardian&&atomic_load(&host.opens)==1:process.pid == resident);
        for (int i = 0; ok && i < 4; i++) {
            ok = replies[i] && strstr(replies[i], subjects[i]) && strstr(replies[i], numbers[i]);
            for (int j = 0; ok && j < 4; j++) if (j != i && strstr(replies[i], subjects[j])) ok = 0;
            if (!ok) fprintf(stderr, "batch isolation failed for %d: %s\n", i, replies[i] ? replies[i] : "<empty>");
        }
        for (int i = 0; i < 4; i++) free(replies[i]);
    }
    if (!ok) fprintf(stderr, "summary transport failed: %s (engine log %s)\n", summary ? summary : "<empty>", log);
    if(samosa_summary_process_close(&process))ok=0;
    if(owned&&!owned_released(&host))ok=0;
    samosa_summary_process_destroy(&process);
    if (ok) {
        printf("summary transport real: PASS (%s)\n", summary);
        unlink(log);
        if(owned){unlink(host.lease);rmdir(root);}
    }
    free(summary);
    return ok ? 0 : 1;
}

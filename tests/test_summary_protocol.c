#include "summary_protocol.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    size_t n = 99;
    unsigned char *p = summary_prompt_frame("hello", 5, &n);
    assert(p && n == 20 && p[3] == 16 && !memcmp(p + 4, "summarize: hello", 16));
    free(p);
    p = summary_opening_frame("hello", 5, &n);
    assert(p && n == 24 && p[0] == 0xc0 && p[1] == 0 && p[2] == 0 && p[3] == 1);
    assert(p[7] == 16 && !memcmp(p + 8, "summarize: hello", 16));
    free(p);
    assert(!summary_opening_frame("", 0, &n) && !n);
    assert(!summary_opening_frame("\xc0\x80", 2, &n));
    assert(!summary_opening_frame("a\0b", 3, &n));
    assert(!summary_opening_frame("hello", 5, NULL));
    assert(!summary_prompt_frame("", 0, &n) && !n);
    assert(!summary_prompt_frame("a\0b", 3, &n));
    assert(!summary_prompt_frame("\xc0\x80", 2, &n));
    char *large = malloc(60001);
    assert(large);
    memset(large, 'x', 60001);
    p = summary_prompt_frame(large, 60000, &n);
    assert(p && n == 60015 && p[0] == 0 && p[1] == 0 && p[2] == 234 && p[3] == 107);
    free(p);
    assert(!summary_prompt_frame(large, 60001, &n));
    free(large);
    unsigned char reply[] = {0, 0, 0, 3, 'a', 'b', 'c'};
    char *s = summary_reply_frame(reply, sizeof(reply), &n);
    assert(s && n == 3 && !strcmp(s, "abc"));
    free(s);
    for (size_t i = 0; i < sizeof(reply); i++)
        assert(!summary_reply_frame(reply, i, &n) && !n);
    assert(!summary_reply_frame(reply, sizeof(reply) + 1, &n));
    reply[4] = 0;
    assert(!summary_reply_frame(reply, sizeof(reply), &n));
    reply[4] = 0xff;
    assert(!summary_reply_frame(reply, sizeof(reply), &n));
    unsigned char *bound = calloc(8197, 1);
    assert(bound);
    bound[2] = 32;
    memset(bound + 4, 'x', 8193);
    s = summary_reply_frame(bound, 8196, &n);
    assert(s && n == 8192);
    free(s);
    bound[3] = 1;
    assert(!summary_reply_frame(bound, 8197, &n));
    bound[0] = 128;
    assert(!summary_reply_frame(bound, 8197, &n));
    free(bound);
    puts("summary protocol boundaries PASS");
    return 0;
}

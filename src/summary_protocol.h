#ifndef SAMOSA_SUMMARY_PROTOCOL_H
#define SAMOSA_SUMMARY_PROTOCOL_H
#include <stddef.h>
/* Existing monolith scalar protocol. Returned buffers belong to the caller.
 * This codec establishes framing only, not engine identity or lifecycle. */
unsigned char *summary_prompt_frame(const char *, size_t, size_t *);
/* Existing batch protocol, one opening sample. Engine partitions the complete
 * prompt into tokenizer-bounded chunks and uses its memory output budget. */
unsigned char *summary_opening_frame(const char *, size_t, size_t *);
char *summary_reply_frame(const unsigned char *, size_t, size_t *);
#endif

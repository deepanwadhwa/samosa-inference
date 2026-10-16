#ifndef SAMOSA_DECISION_TOKENIZER_H
#define SAMOSA_DECISION_TOKENIZER_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum SmTokenizerStatus {
    SM_TOKENIZER_OK = 0, SM_TOKENIZER_INVALID_INPUT = 1,
    SM_TOKENIZER_INVALID_MODEL = 2, SM_TOKENIZER_ENCODING_FAILED = 3,
    SM_TOKENIZER_PANIC = 4, SM_TOKENIZER_CONTEXT_LIMIT = 5
};
typedef struct {
    const uint8_t *data;
    size_t length;
    void *owner;
} SmTokenizerOutput;
/* Caller supplies verified effective tokenizer bytes. Initialize *handle to
 * NULL. The live handle may be encoded concurrently, but not freed concurrently. */
int sm_tokenizer_load(const uint8_t *, size_t, void **handle);
/* Request is bounded UTF-8 JSON. Initialize every output field to zero.
 * On failure outputs remain unchanged; returned JSON owns all three matrices. */
int sm_tokenizer_encode(void *, const uint8_t *, size_t, SmTokenizerOutput *);
/* Free each handle once. Output free clears its fields and is repeatable.
 * Do not alter the opaque owner field or retain data beyond output free. */
void sm_tokenizer_free(void *);
void sm_tokenizer_output_free(SmTokenizerOutput *);
#ifdef __cplusplus
}
#endif
#endif

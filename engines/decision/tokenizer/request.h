#ifndef SAMOSA_DECISION_REQUEST_H
#define SAMOSA_DECISION_REQUEST_H
#include <stddef.h>
#include <stdint.h>
#include "tokenizer.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {const uint8_t *data;size_t length;} SmDecisionText;
typedef struct {SmDecisionText name,description;} SmDecisionCandidate;
typedef struct {SmDecisionText state,proposition,contradiction;} SmDecisionRelation;
enum SmDecisionKind {SM_DECISION_CHOICE=1,SM_DECISION_SCORE=2,SM_DECISION_NOUL=3,
    SM_DECISION_BINARY=4,SM_DECISION_RELATION=5,SM_DECISION_RELATIONS=6};
typedef struct {
    uint32_t kind;
    SmDecisionText state,instructions;
    const SmDecisionCandidate *candidates;size_t candidate_count;
    const SmDecisionText *levels;size_t level_count;
    const SmDecisionRelation *relations;size_t relation_count;
    double threshold;
} SmDecisionRequest;
/* Strict bounded UTF-8 JSON. Initialize *handle=NULL. Status codes match
 * tokenizer.h. A failed load leaves the handle unchanged. */
int sm_decision_request_load(const uint8_t *,size_t,void **handle);
/* All view data is borrowed, read-only, valid until request_free. Text may
 * contain NUL; use lengths. Concurrent view reads are allowed. */
int sm_decision_request_view(const void *,SmDecisionRequest *);
/* Canonical worker JSON after validation, including rendered state and explicit
 * defaults. Preserves candidate order. Zero-initialize output; free with
 * sm_tokenizer_output_free. Failure leaves output unchanged. */
int sm_decision_request_normalized(const void *,SmTokenizerOutput *);
/* Exact typed result schema, order, finite probabilities and consistency with
 * this admitted request. Model-free; returns tokenizer status codes. */
int sm_decision_result_validate(const void *,const uint8_t *,size_t);
/* Explicit modality profile: 0=text, 1=image. The latter requires SigLIP
 * relation metadata and refuses batch relations. Unknown profiles fail. */
int sm_decision_result_validate_profile(const void *,const uint8_t *,size_t,uint32_t);
void sm_decision_request_free(void *);
#ifdef __cplusplus
}
#endif
#endif

#include "summary_protocol.h"
#include "json.h"
#include <stdint.h>

unsigned char *summary_prompt_frame(const char *text, size_t bytes, size_t *length) {
    static const char prefix[] = "summarize: ";
    if (length) *length = 0;
    if (!text || !length || !bytes || bytes > 60000 ||
        memchr(text, 0, bytes) || !utf8_valid((const unsigned char *)text, bytes))
        return NULL;
    size_t payload = sizeof(prefix) - 1 + bytes;
    unsigned char *frame = malloc(payload + 4);
    if (!frame) return NULL;
    frame[0] = (unsigned char)(payload >> 24);
    frame[1] = (unsigned char)(payload >> 16);
    frame[2] = (unsigned char)(payload >> 8);
    frame[3] = (unsigned char)payload;
    memcpy(frame + 4, prefix, sizeof(prefix) - 1);
    memcpy(frame + 4 + sizeof(prefix) - 1, text, bytes);
    *length = payload + 4;
    return frame;
}

unsigned char *summary_opening_frame(const char *text, size_t bytes, size_t *length) {
    if (length) *length = 0;
    if (!length) return NULL;
    size_t scalar_bytes = 0;
    unsigned char *scalar = summary_prompt_frame(text, bytes, &scalar_bytes);
    if (!scalar) return NULL;
    unsigned char *batch = malloc(scalar_bytes + 4);
    if (batch) {
        batch[0] = 0xc0; batch[1] = 0; batch[2] = 0; batch[3] = 1;
        memcpy(batch + 4, scalar, scalar_bytes);
        *length = scalar_bytes + 4;
    }
    free(scalar);
    return batch;
}

char *summary_reply_frame(const unsigned char *frame, size_t bytes, size_t *length) {
    if (length) *length = 0;
    if (!frame || !length || bytes < 4) return NULL;
    uint32_t payload = (uint32_t)frame[0] << 24 | (uint32_t)frame[1] << 16 |
                       (uint32_t)frame[2] << 8 | frame[3];
    if (!payload || payload > 8192 || bytes - 4 != payload ||
        memchr(frame + 4, 0, payload) || !utf8_valid(frame + 4, payload))
        return NULL;
    char *text = malloc((size_t)payload + 1);
    if (!text) return NULL;
    memcpy(text, frame + 4, payload);
    text[payload] = 0;
    *length = payload;
    return text;
}

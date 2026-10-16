#ifndef SAMOSA_RGBA_INPUT_H
#define SAMOSA_RGBA_INPUT_H
#include <stdlib.h>
#include <string.h>
static inline int rgba_base64_value(unsigned char c) {
    if (c >= 'A' && c <= 'Z')
        return c - 'A';
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 26;
    if (c >= '0' && c <= '9')
        return c - '0' + 52;
    return c == '+' ? 62 : c == '/' ? 63 : -1;
}
static inline unsigned char *rgba_decode(const char *s, size_t expected) {
    size_t n = strlen(s);
    if (n != ((expected + 2) / 3) * 4 || expected > (512u * 512u * 4u))
        return NULL;
    unsigned char *bytes = malloc(n / 4 * 3);
    if (!bytes)
        return NULL;
    size_t used = 0;
    for (size_t i = 0; i < n; i += 4) {
        int a = rgba_base64_value((unsigned char)s[i]), b = rgba_base64_value((unsigned char)s[i + 1]),
            c = rgba_base64_value((unsigned char)s[i + 2]), d = rgba_base64_value((unsigned char)s[i + 3]),
            pc = s[i + 2] == '=', pd = s[i + 3] == '=';
        if (a < 0 || b < 0 || (!pc && c < 0) || (!pd && d < 0) || (pc && !pd) ||
            ((pc || pd) && i + 4 != n) || (pc && (b & 15)) || (!pc && pd && (c & 3))) {
            free(bytes);
            return NULL;
        }
        bytes[used++] = (unsigned char)((a << 2) | (b >> 4));
        if (!pc)
            bytes[used++] = (unsigned char)((b << 4) | (c >> 2));
        if (!pd)
            bytes[used++] = (unsigned char)((c << 6) | d);
    }
    if (used != expected) {
        free(bytes);
        return NULL;
    }
    return bytes;
}
#endif

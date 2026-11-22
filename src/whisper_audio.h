#ifndef SAMOSA_WHISPER_AUDIO_H
#define SAMOSA_WHISPER_AUDIO_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#define WHISPER_WAV_MAX 960044u
static inline uint32_t whisper_wav_u32(const unsigned char *p) {
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static inline uint16_t whisper_wav_u16(const unsigned char *p) {
    return (uint16_t)((unsigned)p[0] | (unsigned)p[1]<<8);
}
static inline int whisper_canonical_wav(const unsigned char *p, size_t n) {
    return n>=76 && n<=WHISPER_WAV_MAX && !((n-44)%32) &&
        !memcmp(p,"RIFF",4) && whisper_wav_u32(p+4)==n-8 && !memcmp(p+8,"WAVEfmt ",8) &&
        whisper_wav_u32(p+16)==16 && whisper_wav_u16(p+20)==1 && whisper_wav_u16(p+22)==1 &&
        whisper_wav_u32(p+24)==16000 && whisper_wav_u32(p+28)==32000 &&
        whisper_wav_u16(p+32)==2 && whisper_wav_u16(p+34)==16 && !memcmp(p+36,"data",4) &&
        whisper_wav_u32(p+40)==n-44;
}
#endif

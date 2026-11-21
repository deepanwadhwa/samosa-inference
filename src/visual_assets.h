#ifndef SAMOSA_INFERENCE_VISUAL_ASSETS_H
#define SAMOSA_INFERENCE_VISUAL_ASSETS_H
#include "vision.h"
#include "temporal.h"
#include <stdint.h>
#include <sys/stat.h>
typedef struct { struct stat directory, files[3]; } VisionModelReceipt;
int visual_same_receipt(const struct stat *,const struct stat *);
const char *vision_file_verify(const InferenceConfig *,int,const char *,uint64_t,int);
const char *vision_model_verify(const InferenceConfig *,int,VisionModelReceipt *,int);
const char *temporal_engine_verify(const InferenceConfig *,int,struct stat *,int);
const char *visual_write_owned(const InferenceConfig *,int,const char *,const void *,size_t);
/* Existing lossless RGBA-to-white-alpha-P6 preparation and public reply checks. */
char *vision_ppm(const unsigned char *,size_t,size_t,size_t *,char [65]);
const char *vision_result(const char *,const char *,size_t,char **,size_t *,size_t *);
const char *temporal_result(const char *,const TemporalInput *,char **,size_t *,size_t *);
/* Existing request encoders; their workspace paths are trusted owner inputs. */
char *vision_command(const char *,const char *,const char *,size_t,size_t *);
char *temporal_command(const char *,const TemporalInput *,size_t *);
#endif

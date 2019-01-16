#ifndef __NATIVE_AUDIO__H
#define __NATIVE_AUDIO__H
#ifdef __cplusplus
extern "C" {
#endif

#include "opensl_io.h"
typedef void (*pcmOutCallBack)(uint8_t *buff, uint32_t size);
int startAudioCapture(pcmOutCallBack _cb);
void stopAudioCapture();
void startPlay();
void stopPlay();

#ifdef __cplusplus
}
#endif
#endif

#ifndef __PRERECORD__H
#define __PRERECORD__H
#ifdef __cplusplus
extern "C" {
#endif

#include "mediacodec.h"
#include "queue.h"

typedef struct _RecordFrame
{
	uint32_t size;
	uint8_t *buff;
	AMediaCodecBufferInfo info;
}RecordFrame;

enum {
	ENCODE_VIDEO_TYPE = 0,
	ENCODE_AUDIO_TYPE = 1,
};

int64_t nanoTime;

#define PreRecord_LOGD(...) __android_log_print(ANDROID_LOG_DEBUG , "PreRecord", __VA_ARGS__)
#define PreRecord_LOGI(...) __android_log_print(ANDROID_LOG_INFO , "PreRecord", __VA_ARGS__)
#define PreRecord_LOGE(...) __android_log_print(ANDROID_LOG_ERROR , "PreRecord", __VA_ARGS__)

int preparePreRecord(uint32_t video_w, uint32_t video_h, uint32_t video_framerate, uint32_t video_bitrate,
				   uint32_t audio_sample_rate, uint8_t  audio_aac_profile, uint32_t audio_bitrate, uint8_t  audio_channel_count, uint32_t duration);

int startPreRecord();

int stopPreRecord();

int Encode(uint8_t *in, uint32_t in_len, uint8_t encode_type);

int dumpQueueDataToMP4();

#ifdef __cplusplus
}
#endif
#endif

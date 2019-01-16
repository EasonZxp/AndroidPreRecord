#include <assert.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <stdbool.h>
#include <pthread.h>
#include <android/log.h>
#include <binder/ProcessState.h>
#include "preRecord.h"

using namespace android;

MediaCodecEncoder* video_encoder = NULL;
uint8_t *video_tmp_out = NULL;
queue_t *gVideoQueue = NULL;
queue_t *gVideoHeadQueue = NULL;
int mVideoTrack = -1;
uint64_t video_last_encode_frame_time = 0;
uint64_t video_tmp_time = 0;

MediaCodecEncoder* audio_encoder = NULL;
uint8_t *audio_tmp_out = NULL;
queue_t *gAudioQueue = NULL;
queue_t *gAudioHeadQueue = NULL;
AMediaMuxer *mp4_muxer = NULL;
int mAudioTrack = -1;
uint64_t audio_last_encode_frame_time = 0;
uint64_t audio_tmp_time = 0;


static int init_muxer()
{
	int fd = open("/sdcard/queue.mp4", O_CREAT | O_LARGEFILE | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR);
	if (fd < 0) {
		PreRecord_LOGE("ERROR: couldn't open file\n");
		return -1;
	}
	mp4_muxer = AMediaMuxer_new(fd, AMEDIAMUXER_OUTPUT_FORMAT_MPEG_4);
	if(mp4_muxer != NULL)
	{
		PreRecord_LOGI("AMediaMuxer new successfully...\n");
	}
	close(fd);
	return 0;
}

static void deinit_muxer()
{
	if (mp4_muxer != NULL)
	{
		AMediaMuxer_stop(mp4_muxer);
		AMediaMuxer_delete(mp4_muxer);
		mp4_muxer = NULL;
	}
}

void mediaCodecVideoFormatChangedCb(AMediaFormat *format)
{
	if(!format || !mp4_muxer)
	{
		PreRecord_LOGE("in %s, format null or mp4_muxer null\n", __func__);
		return;
	}

	mVideoTrack = (size_t) AMediaMuxer_addTrack(mp4_muxer, format);
	PreRecord_LOGI("%s: mVideoTrack = %d\n", __func__, mVideoTrack);
	mVideoTrack = 1;
#if 1
	if(mVideoTrack < 0 || mAudioTrack < 0)
	{
		PreRecord_LOGE("%s, AmediaMuxer addTrack failed: mVideoTrack:%d, mAudioTrack:%d\n", __func__, mVideoTrack, mAudioTrack);
		return;
	}
#endif
	if(AMEDIA_OK !=AMediaMuxer_start(mp4_muxer)){
		PreRecord_LOGE("%s, AmediaMuxer start failed...\n", __func__);
		return;
	}
	PreRecord_LOGI("%s, AmediaMuxer start ok...\n", __func__);
}

void mediaCodecAudioFormatChangedCb(AMediaFormat *format)
{
	if(!format || !mp4_muxer)
	{
		PreRecord_LOGE("in %s, format null or mp4_muxer null\n", __func__);
		return;
	}

	mAudioTrack = (size_t) AMediaMuxer_addTrack(mp4_muxer, format);
	PreRecord_LOGI("%s: mAudioTrack = %d\n", __func__, mAudioTrack);
	mAudioTrack = 0;
#if 1
	if(mVideoTrack < 0 || mAudioTrack < 0)
	{
		PreRecord_LOGE("%s, AmediaMuxer addTrack failed: mVideoTrack:%d, mAudioTrack:%d\n", __func__, mVideoTrack, mAudioTrack);
		return;
	}
#endif
	if(AMEDIA_OK !=AMediaMuxer_start(mp4_muxer)){
		PreRecord_LOGE("%s, AmediaMuxer start failed...\n", __func__);
		return;
	}
	PreRecord_LOGI("%s, AmediaMuxer start ok...\n", __func__);
}

static void free_frame(RecordFrame *frame) {
	if(frame !=NULL && frame->buff != NULL)
	{
		free(frame->buff);
		free(frame);
		frame = NULL;
	}
}

int dumpQueueDataToMP4()
{
	int i = 0;
	int num = 0;
	RecordFrame *tmp_frame;

	num = queue_elements(gVideoHeadQueue);
	if(video_encoder->DEBUG)
		PreRecord_LOGI("[DUMP_VIDEO]: num %d frame in head queue...\n", num);
	for(i=0; i<num; i++){
		queue_get(gVideoHeadQueue, (void **)&tmp_frame);
		AMediaMuxer_writeSampleData(mp4_muxer, mVideoTrack, tmp_frame->buff, &(tmp_frame->info));
		if(video_encoder->DEBUG)
			PreRecord_LOGI("[DUMP_VIDEO]: HeadQueue write, queue_get:%p, size:%d, time:%lld, flags:%d\n", tmp_frame, tmp_frame->info.size, tmp_frame->info.presentationTimeUs, tmp_frame->info.flags);
		free_frame(tmp_frame);
	}

	num = queue_elements(gVideoQueue);
	if(video_encoder->DEBUG)
		PreRecord_LOGI("[DUMP_VIDEO]: num %d frame in major queue...\n", num);
	for(i=0; i<num; i++){
		queue_get(gVideoQueue, (void **)&tmp_frame);
		if(video_last_encode_frame_time >= video_encoder->duration * 1000 * 1000)
			tmp_frame->info.presentationTimeUs = tmp_frame->info.presentationTimeUs - video_tmp_time;
			//tmp_frame->info.presentationTimeUs -= (video_last_encode_frame_time - video_encoder->duration * 1000 * 1000);
		AMediaMuxer_writeSampleData(mp4_muxer, mVideoTrack, tmp_frame->buff, &(tmp_frame->info));
		if(video_encoder->DEBUG)
			PreRecord_LOGI("[DUMP_VIDEO]: MajorQueue write, queue_get:%p, size:%d, time:%lld, flags:%d\n", tmp_frame, tmp_frame->info.size, tmp_frame->info.presentationTimeUs, tmp_frame->info.flags);
		free_frame(tmp_frame);
	}

	num = queue_elements(gAudioHeadQueue);
	if(audio_encoder->DEBUG)
		PreRecord_LOGI("[DUMP_AUDIO]: num %d frame in head queue...\n", num);
	for(i=0; i<num; i++){
		queue_get(gAudioHeadQueue, (void **)&tmp_frame);
		AMediaMuxer_writeSampleData(mp4_muxer, mAudioTrack, tmp_frame->buff, &(tmp_frame->info));
		if(audio_encoder->DEBUG)
			PreRecord_LOGI("[DUMP_AUDIO]: HeadQueue write, queue_get:%p, size:%d, time:%lld, flags:%d\n", tmp_frame, tmp_frame->size, tmp_frame->info.presentationTimeUs, tmp_frame->info.flags);
		free_frame(tmp_frame);
	}

	tmp_frame = NULL;
	num = queue_elements(gAudioQueue);
	if(audio_encoder->DEBUG)
		PreRecord_LOGI("[DUMP_AUDIO]: num %d frame in major queue...\n", num);
	for(i=0; i<num; i++){
		queue_get(gAudioQueue, (void **)&tmp_frame);
		if(audio_last_encode_frame_time >= audio_encoder->duration * 1000 * 1000)
			tmp_frame->info.presentationTimeUs = tmp_frame->info.presentationTimeUs - audio_tmp_time;
		AMediaMuxer_writeSampleData(mp4_muxer, mAudioTrack, tmp_frame->buff, &(tmp_frame->info));
		if(audio_encoder->DEBUG)
			PreRecord_LOGI("[DUMP_AUDIO]: MajorQueue write, queue_get:%p, size:%d, time:%lld, flags:%d\n", tmp_frame, tmp_frame->size, tmp_frame->info.presentationTimeUs, tmp_frame->info.flags);
		free_frame(tmp_frame);
	}


	return 0;
}

static int64_t systemnanotime() {
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return now.tv_sec * 1000000000LL + now.tv_nsec;
}

static int is_encoder_started = 0;
int preparePreRecord(uint32_t video_w, uint32_t video_h, uint32_t video_framerate, uint32_t video_bitrate,
				   uint32_t audio_sample_rate, uint8_t  audio_aac_profile, uint32_t audio_bitrate, uint8_t  audio_channel_count, uint32_t duration)
{
	int ret = 0;

	(void)video_w;
	(void)video_h;
	(void)video_framerate;
	(void)video_bitrate;
	ProcessState::self()->startThreadPool();
	PreRecord_LOGI("MediaCodec: preparePreRecord....:%d\n", duration);

	ret = init_muxer();
	if(ret)
	{
		PreRecord_LOGE("init codec muxer failed...\n");
		return ret;
	}

	gVideoQueue = queue_create_limited(video_framerate * duration);
	gVideoHeadQueue = queue_create();
	if(!gVideoQueue || !gVideoHeadQueue)
	{
		PreRecord_LOGE("PreRecord queue create failed...\n");
		return -2;
	}
	gAudioQueue = queue_create_limited(duration * 43);
	gAudioHeadQueue = queue_create();
	if(!gAudioQueue || !gAudioHeadQueue)
	{
		PreRecord_LOGE("PreRecord queue create failed...\n");
		return -2;
	}

	video_encoder = mediacodec_encoder_alloc(0, video_w, video_h, video_framerate, video_bitrate, 1000, NV21);
	video_encoder->duration = duration;
	video_encoder->_format_changed_cb = mediaCodecVideoFormatChangedCb;
	audio_encoder = mediacodec_audio_encoder_alloc(0, audio_sample_rate, audio_aac_profile, audio_bitrate, audio_channel_count);
	audio_encoder->duration = duration;
	audio_encoder->_format_changed_cb = mediaCodecAudioFormatChangedCb;

	ret = mediacodec_encoder_open(video_encoder);
	if(ret)
	{
		PreRecord_LOGI("MediaCodec: video_encoder_open failed....:\n");
		return -3;
	}
	ret = mediacodec_encoder_open(audio_encoder);
	if(ret)
	{
		PreRecord_LOGI("MediaCodec: audio_encoder_open failed....:\n");
		return -3;
	}

	video_tmp_out = (uint8_t *)malloc(video_w*video_h*3/2);
	audio_tmp_out = (uint8_t *)malloc(1024*1024);
	return 0;
}

int startPreRecord()
{
	int status = 0;

	is_encoder_started = 1;

	nanoTime = systemnanotime();

	status = mediacodec_encoder_start(video_encoder);
	if(status)
	{
		PreRecord_LOGI("[start]: video encoder start failed...\n");
		return status;
	}

	status = mediacodec_encoder_start(audio_encoder);
	if(status)
	{
		PreRecord_LOGI("[start]: audio encoder start failed...\n");
		return status;
	}

	return status;
}

int stopPreRecord()
{
	int ret = 0;

	PreRecord_LOGI("MediaCodec: stopPreRecord....:\n");

	is_encoder_started = 0;
	deinit_muxer();
	ret = mediacodec_encoder_close(video_encoder);
	if(ret)
	{
		PreRecord_LOGI("MediaCodec: mediacodec_encoder_close failed....:\n");
		return -1;
	}
	ret = mediacodec_encoder_close(audio_encoder);
	if(ret)
	{
		PreRecord_LOGI("MediaCodec: mediacodec_encoder_close failed....:\n");
		return -1;
	}

	mediacodec_encoder_free(video_encoder);
	mediacodec_encoder_free(audio_encoder);

	queue_destroy_complete(gVideoHeadQueue, NULL);
	queue_destroy_complete(gVideoQueue, NULL);
	queue_destroy_complete(gAudioHeadQueue, NULL);
	queue_destroy_complete(gAudioQueue, NULL);

	free(video_tmp_out);
	free(audio_tmp_out);

	video_tmp_out = NULL;
	audio_tmp_out = NULL;

	return 0;
}

int Encode(uint8_t *in, uint32_t in_len, uint8_t encode_type)
{
	int out_len = 0;
	int jerror_code;
	uint8_t *tmp_buf;
	AMediaCodecBufferInfo info;

	if(!is_encoder_started){
		PreRecord_LOGI("[Encode]: encoder not start yet...");
		return -1;
	}
	if(encode_type == ENCODE_VIDEO_TYPE)
	{
		out_len = mediacodec_encoder_encode(video_encoder, in, 0, video_tmp_out, in_len, &jerror_code, &info);

		if(out_len <= 0)
			return jerror_code;

		//PreRecord_LOGI("[VIDEO]: out_len:%6d, info.size %d", out_len, info.size);
		if(info.presentationTimeUs == 0){
			RecordFrame *head_frame = (RecordFrame *)malloc(sizeof(RecordFrame));
			head_frame->size = out_len;
			tmp_buf = (uint8_t *)malloc(out_len);
			head_frame->buff = tmp_buf;
			head_frame->info = info;
			memcpy(head_frame->buff, video_tmp_out, out_len);
			if (video_encoder->DEBUG) {
				PreRecord_LOGI("[VIDEO]: HeadQueue put:%p, size:%6d, time:%lld", head_frame, out_len, info.presentationTimeUs);
			}
			queue_put(gVideoHeadQueue, head_frame);
		}else{
			RecordFrame *record_frame = (RecordFrame *)malloc(sizeof(RecordFrame));
			record_frame->size = out_len;
			tmp_buf = (uint8_t *)malloc(out_len);
			record_frame->buff = tmp_buf;
			record_frame->info = info;
			memcpy(record_frame->buff, video_tmp_out, out_len);

			//queue is full?
			unsigned int num = queue_elements(gVideoQueue);
			if(num < (video_encoder->frame_rate * video_encoder->duration)){
				if (video_encoder->DEBUG) {
					PreRecord_LOGI("[VIDEO]: MajorQueue: idx:%lld, queue_put :%p, size:%d, time:%lld\n", video_encoder->frameIndex, record_frame, record_frame->size, record_frame->info.presentationTimeUs);
				}
				queue_put(gVideoQueue, record_frame);
			}else{
				RecordFrame *tmp_frame;
				queue_get(gVideoQueue, (void **)&tmp_frame);
				video_tmp_time = tmp_frame->info.presentationTimeUs;
				if (video_encoder->DEBUG) {
					PreRecord_LOGI("[VIDEO]:MajorQueue: queue_get :%p, size:%d, time:%lld\n", tmp_frame, tmp_frame->size, tmp_frame->info.presentationTimeUs);
				}
				free_frame(tmp_frame);
				queue_put(gVideoQueue, record_frame);
				video_last_encode_frame_time = info.presentationTimeUs;
				if (video_encoder->DEBUG) {
					PreRecord_LOGI("[VIDEO]:MajorQueue: queue_put :%p, size:%d, time:%lld\n", record_frame, record_frame->size, record_frame->info.presentationTimeUs);
				}
			}
		}
	}else if(encode_type == ENCODE_AUDIO_TYPE){
		out_len = mediacodec_encoder_encode(audio_encoder, in, 0, audio_tmp_out, in_len, &jerror_code, &info);
		if(out_len <= 0)
			return jerror_code;

		//PreRecord_LOGI("[AUDIO]: out_len:%6d, info.size %d", out_len, info.size);
		if(info.presentationTimeUs == 0){
			RecordFrame *head_frame = (RecordFrame *)malloc(sizeof(RecordFrame));
			head_frame->size = out_len;
			tmp_buf = (uint8_t *)malloc(out_len);
			head_frame->buff = tmp_buf;
			head_frame->info = info;
			memcpy(head_frame->buff, audio_tmp_out, out_len);
			if (audio_encoder->DEBUG) {
				PreRecord_LOGI("[AUDIO]: HeadQueue put:%p, size:%6d, time:%lld", head_frame, out_len, info.presentationTimeUs);
			}
			queue_put(gAudioHeadQueue, head_frame);
		}else{
			RecordFrame *record_frame = (RecordFrame *)malloc(sizeof(RecordFrame));
			record_frame->size = out_len;
			tmp_buf = (uint8_t *)malloc(out_len);
			record_frame->buff = tmp_buf;
			record_frame->info = info;
			memcpy(record_frame->buff, audio_tmp_out, out_len);

			//queue is full?
			unsigned int num = queue_elements(gAudioQueue);
			if(num < audio_encoder->duration * 43){
				queue_put(gAudioQueue, record_frame);
				if (audio_encoder->DEBUG) {
					PreRecord_LOGI("[AUDIO]: MajorQueue: idx:%lld, queue_put :%p, size:%d, time:%lld\n", audio_encoder->frameIndex, record_frame, record_frame->size, record_frame->info.presentationTimeUs);
				}
			}else{
				RecordFrame *tmp_frame;
				queue_get(gAudioQueue, (void **)&tmp_frame);
				audio_tmp_time = tmp_frame->info.presentationTimeUs;
				if (audio_encoder->DEBUG) {
					PreRecord_LOGI("[AUDIO]:MajorQueue: idx:%lld queue_get :%p, size:%d, time:%lld\n", audio_encoder->frameIndex, tmp_frame, tmp_frame->size, tmp_frame->info.presentationTimeUs);
				}
				free_frame(tmp_frame);
				queue_put(gAudioQueue, record_frame);
				audio_last_encode_frame_time = info.presentationTimeUs;
				if (audio_encoder->DEBUG) {
					PreRecord_LOGI("[AUDIO]:MajorQueue: queue_put :%p, size:%d, time:%lld\n", record_frame, record_frame->size, record_frame->info.presentationTimeUs);
				}
			}
		}
	}else{
		PreRecord_LOGE("Error encode type...");
		return -1;
	}

	return 0;
}

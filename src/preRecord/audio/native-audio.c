#include <stdio.h>
#include <android/log.h>
#include <pthread.h>
#include "native-audio.h"

#define LOG(...) __android_log_print(ANDROID_LOG_DEBUG,"native-audio",__VA_ARGS__)

#define SAMPLERATE 44100
#define CHANNELS 1
#define PERIOD_TIME 40 //ms
#define FRAME_SIZE SAMPLERATE*PERIOD_TIME/1000
#define BUFFER_SIZE FRAME_SIZE*CHANNELS
#define TEST_CAPTURE_FILE_PATH "/sdcard/audio.pcm"

static volatile int g_loop_exit = 0;
OPENSL_STREAM* stream = NULL;
pthread_t AudioCapThreadID;

FILE *pcm_fp = NULL;
int openTestPcmFile()
{
	pcm_fp = fopen(TEST_CAPTURE_FILE_PATH, "wb");
	if (pcm_fp == NULL) {
		LOG("open file: (%s)\n failed", TEST_CAPTURE_FILE_PATH);
		return -1;
	}
	return 0;
}

int saveTestPcmFile(uint8_t *buf, uint32_t size){
	if (fwrite(buf, size, 1, pcm_fp) != 1) {
		LOG("write record data failed");
		return -1;
	}
	return 0;
}

int closeTestPcmFile()
{
	fclose(pcm_fp);
	return 0;
}

static void *_threadAudioCap(void* arg)
{
	LOG("start capture thread....\n");
	pcmOutCallBack _cb = (pcmOutCallBack)arg;
	int samples;
	uint8_t buffer[BUFFER_SIZE]; //存储缓冲区

	while(!g_loop_exit) {
		samples = android_AudioIn(stream, buffer, BUFFER_SIZE);
		if (samples < 0) {
			LOG("recording is failed, please check!");
			break;
		}
		LOG("record %d samples !\n ", samples);
		if(_cb != NULL)
		{
			//LOG("_cb != NULL\n ");
			_cb(buffer, samples);
		}
		//saveTestPcmFile(buffer, samples);
		//here do the encode?
	}
	LOG("stop capture thread....\n");

	return NULL;
}

int startAudioCapture(pcmOutCallBack _cb) {
	//int ret = 0;
	if(!_cb)
	{
		LOG("pcmOutCallBack NULL...");
		return -1;
	}
	//ret = openTestPcmFile();

	stream = android_OpenAudioDevice(SAMPLERATE, CHANNELS, CHANNELS, FRAME_SIZE);
	if (stream == NULL) {
		LOG("open record stream fail");
		return -2;
	}

	if(pthread_create(&AudioCapThreadID, NULL, _threadAudioCap, _cb) != 0) {
		LOG("Create capThread failed!\n");
		return -3;
	}

	return 0;
}

void stopAudioCapture()
{
    void *dummy;
	g_loop_exit = 1;
    pthread_join(AudioCapThreadID, &dummy);

	//清理资源
	if(stream != NULL)
	{
		android_CloseAudioDevice(stream);
		stream = NULL;
	}
	//closeTestPcmFile();
}

void startPlay() {
	LOG("start Play...");
	FILE * fp = fopen(TEST_CAPTURE_FILE_PATH, "rb");
	if( fp == NULL ) {
		LOG("cannot open file (%s) !\n",TEST_CAPTURE_FILE_PATH);
		return;
	}

	//打开流
	OPENSL_STREAM* stream = android_OpenAudioDevice(SAMPLERATE, CHANNELS, CHANNELS, FRAME_SIZE);
	if (stream == NULL) {
		fclose(fp);
		LOG("failed to open audio device ! \n");
		return;
	}

	int samples;
	uint8_t buffer[BUFFER_SIZE];
	g_loop_exit = 0;
	while (!g_loop_exit && !feof(fp)) {
		//读文件
		if (fread((unsigned char *)buffer, BUFFER_SIZE, 1, fp) != 1) {
			LOG("failed to read data \n ");
			break;
		}
		//播放
		samples = android_AudioOut(stream, buffer, BUFFER_SIZE);
		if (samples < 0) {
			LOG("android_AudioOut failed !\n");
		}
		LOG("playback %d samples !\n", samples);
	}
	android_CloseAudioDevice(stream);
	fclose(fp);

	LOG("nativeStartPlayback completed !");

	return;
}

void stopPlay() {
	g_loop_exit = 1;
	return;
}

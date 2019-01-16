#include "mediacodec.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

int64_t systemnanotime() {
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return now.tv_sec * 1000000000LL + now.tv_nsec;
}

MediaCodecEncoder* mediacodec_encoder_alloc(int isDebug, int width, int height, int frame_rate, int bit_rate, int timeout, YUV_PIXEL_FORMAT yuv_pixel_format) {
	if (isDebug) {
		MediaCodec_LOGI("[video mediacodec alloc]");
	}
	MediaCodecEncoder* encoder = (MediaCodecEncoder*)malloc(sizeof(MediaCodecEncoder));
	encoder->codec = NULL;

	encoder->mime = NULL;
	encoder->colorFormat = 0;

	encoder->DEBUG = isDebug;
	encoder->TIME_OUT = timeout;
	encoder->MAX_TIME_OUT = 0;
	encoder->MIMETYPE_VIDEO_AVC = "video/hevc";//只有高通cpu支持hevc-encoder
	encoder->profile = 0;
	encoder->level = 0;
	encoder->yuv_pixel_format = yuv_pixel_format;
	encoder->sps = NULL;
	encoder->sps_length = 0;
	encoder->pps = NULL;
	encoder->pps_length = 0;
	encoder->startCode = "\0\0\0\1";
	encoder->startCode_length = 4;
	encoder->firstI = 0;

	encoder->width = width;
	encoder->height = height;
	encoder->frame_rate = frame_rate;
	encoder->bit_rate = bit_rate;
	encoder->frameIndex = 0;

	encoder->SDK_INT = 0;
	encoder->phone_type = NULL;
	encoder->hardware = NULL;
	encoder->duration = 0;
	encoder->_format_changed_cb = NULL;
	encoder->MIMETYPE_AUDIO = "";
	encoder->is_audio_type = 0;

	return encoder;
}

MediaCodecEncoder* mediacodec_audio_encoder_alloc(int isDebug, uint32_t sample_rate, uint8_t  aac_profile, uint32_t audio_bitrate, uint8_t  channel_count) {
	if (isDebug) {
		MediaCodec_LOGI("[audio mediacodec alloc]");
	}
	MediaCodecEncoder* encoder = (MediaCodecEncoder*)malloc(sizeof(MediaCodecEncoder));
	encoder->codec = NULL;

	encoder->mime = NULL;
	encoder->colorFormat = 0;

	encoder->DEBUG = isDebug;
	encoder->TIME_OUT = 0;
	encoder->MAX_TIME_OUT = 0;
	encoder->MIMETYPE_VIDEO_AVC = "";
	encoder->profile = 0;
	encoder->level = 0;
	encoder->yuv_pixel_format = 0;
	encoder->sps = NULL;
	encoder->sps_length = 0;
	encoder->pps = NULL;
	encoder->pps_length = 0;
	encoder->startCode = NULL;
	encoder->startCode_length = 0;
	encoder->firstI = 0;

	encoder->width = 0;
	encoder->height = 0;
	encoder->frame_rate = 0;
	encoder->bit_rate = 0;
	encoder->frameIndex = 0;

	encoder->SDK_INT = 0;
	encoder->phone_type = NULL;
	encoder->hardware = NULL;
	encoder->duration = 0;
	encoder->_format_changed_cb = NULL;

	//audio
	encoder->MIMETYPE_AUDIO = "audio/mp4a-latm";
	encoder->sample_rate = sample_rate;
	encoder->aac_profile = aac_profile;
	encoder->audio_bitrate = audio_bitrate;
	encoder->channel_count = channel_count;
	encoder->is_audio_type = 1;

	return encoder;
}

int mediacodec_encoder_free(MediaCodecEncoder* encoder) {
	if(encoder){
		if (encoder->DEBUG) {
			MediaCodec_LOGI("[free]");
		}
		free(encoder);
		return 0;
	}
	else{
		if (encoder->DEBUG) {
			MediaCodec_LOGE("[free]ERROR_SDK_ENCODER_IS_NULL");
		}
		return -1;
	}
}

int mediacodec_encoder_flush(MediaCodecEncoder* encoder) {
	if(encoder){
		if (encoder->DEBUG) {
			MediaCodec_LOGI("[flush]");
		}

		int status;

		status = AMediaCodec_flush(encoder->codec);
		if(status){
			if (encoder->DEBUG) {
				MediaCodec_LOGE("[flush]AMediaCodec_flush error");
			}
		}
		return status;
	}
	else{
		if (encoder->DEBUG) {
			MediaCodec_LOGE("[flush]ERROR_SDK_ENCODER_IS_NULL");
		}
		return -1;
	}
}

int mediacodec_encoder_open(MediaCodecEncoder* encoder) {
	if(!encoder){
		if (encoder->DEBUG) {
			MediaCodec_LOGE("[open]ERROR_SDK_ENCODER_IS_NULL");
		}
		return -1;
	}

	if (encoder->DEBUG) {
		MediaCodec_LOGI("[open]");
	}

	char sdk[10] = {0};
	__system_property_get("ro.build.version.sdk",sdk);
	encoder->SDK_INT = atoi(sdk);

	if(encoder-> SDK_INT >= 16){
		if (encoder->DEBUG) {
			MediaCodec_LOGI("[open]SDK_INT : %d", encoder->SDK_INT);
		}
		if(encoder-> SDK_INT >= 21){
			encoder->MAX_TIME_OUT = 100;
		}
		else{
			encoder->MAX_TIME_OUT = 200;
		}
	}else{
		if (encoder->DEBUG) {
			MediaCodec_LOGE("[open]ERROR_SDK_VERSION_TOO_LOW  SDK_INT : %d", encoder->SDK_INT);
		}
		return -2;//ERROR_SDK_VERSION_TOO_LOW
	}

	char phone[20] = {0};
	__system_property_get("ro.product.model",phone);
	encoder->phone_type = (char*)malloc(20);
	memmove(encoder->phone_type, phone, 20);
	if (encoder->DEBUG) {
		MediaCodec_LOGI("[open]phone_type : %s", encoder->phone_type);
	}

	char cpu[20] = {0};
	__system_property_get("ro.hardware",cpu);
	encoder->hardware = (char*)malloc(20);
	memmove(encoder->hardware, cpu, 20);
	if (encoder->DEBUG) {
		MediaCodec_LOGI("[open]hardware : %s", encoder->hardware);
	}

	int status = 0;
	AMediaFormat* mediaFormat = NULL;

	if(encoder->is_audio_type == 0)//type video
	{
		encoder->codec = AMediaCodec_createEncoderByType(encoder->MIMETYPE_VIDEO_AVC);
		mediaFormat = AMediaFormat_new();
		AMediaFormat_setString(mediaFormat, AMEDIAFORMAT_KEY_MIME, encoder->MIMETYPE_VIDEO_AVC);
		AMediaFormat_setInt32(mediaFormat, AMEDIAFORMAT_KEY_WIDTH, encoder->width);
		AMediaFormat_setInt32(mediaFormat, AMEDIAFORMAT_KEY_HEIGHT, encoder->height);
		AMediaFormat_setInt32(mediaFormat, AMEDIAFORMAT_KEY_BIT_RATE, encoder->bit_rate);
		AMediaFormat_setInt32(mediaFormat, "max-bitrate", encoder->bit_rate * 2);
		AMediaFormat_setInt32(mediaFormat, "bitrate-mode", 2);//BITRATE_MODE_CBR
		AMediaFormat_setInt32(mediaFormat, AMEDIAFORMAT_KEY_FRAME_RATE, encoder->frame_rate);

		if(encoder->hardware[0] == 'm' && encoder->hardware[1] == 't'){//mtk cpu输入为I420，其余cpu为NV12
			AMediaFormat_setInt32(mediaFormat, AMEDIAFORMAT_KEY_COLOR_FORMAT, 19);//COLOR_FormatYUV420Planar
		}
		else{
			AMediaFormat_setInt32(mediaFormat, AMEDIAFORMAT_KEY_COLOR_FORMAT, 21);//COLOR_FormatYUV420SemiPlanar
		}

		AMediaFormat_setInt32(mediaFormat, AMEDIAFORMAT_KEY_I_FRAME_INTERVAL, 1);//GOP 1s
#if 1
		uint8_t sps[2] = {0x12, 0x12};
		uint8_t pps[2] = {0x12, 0x12};
		AMediaFormat_setBuffer(mediaFormat, "csd-0", sps, 2); // sps
		AMediaFormat_setBuffer(mediaFormat, "csd-1", pps, 2); // pps
#endif
		if(!strcmp("video/avc",encoder->MIMETYPE_VIDEO_AVC)){
			encoder->profile = 0x08;//AVCProfileHigh

			encoder->level = 0x100;//AVCLevel30
			if(encoder->width * encoder->height >= 1280 * 720){
				encoder->level = 0x200;//AVCLevel31
			}
			if(encoder->width * encoder->height >= 1920 * 1080){
				encoder->level = 0x800;//AVCLevel40
			}
		}
		else if(!strcmp("video/hevc",encoder->MIMETYPE_VIDEO_AVC)){
			encoder->profile =  0x01;//HEVCProfileMain

			encoder->level = 0x80;//HEVCHighTierLevel30
			if(encoder->width * encoder->height >= 1280 * 720){
				encoder->level = 0x200;//HEVCHighTierLevel31
			}
			if(encoder->width * encoder->height >= 1920 * 1080){
				encoder->level = 0x800;//HEVCHighTierLevel40
			}
		}
		if (encoder->DEBUG) {
			MediaCodec_LOGI("[open]AMEDIAFORMAT_KEY_MIME : %s  profile : %x  level : %x", encoder->MIMETYPE_VIDEO_AVC, encoder->profile, encoder->level);
		}

		AMediaFormat_setInt32(mediaFormat, "profile", encoder->profile);
		AMediaFormat_setInt32(mediaFormat, "level", encoder->level);

		encoder->firstI = 0;
		encoder->frameIndex = 0;
	}else if(encoder->is_audio_type == 1){
		encoder->codec = AMediaCodec_createEncoderByType(encoder->MIMETYPE_AUDIO);
		mediaFormat = AMediaFormat_new();
		AMediaFormat_setString(mediaFormat, AMEDIAFORMAT_KEY_MIME, encoder->MIMETYPE_AUDIO);
		AMediaFormat_setInt32(mediaFormat, AMEDIAFORMAT_KEY_SAMPLE_RATE, encoder->sample_rate);
		AMediaFormat_setInt32(mediaFormat, AMEDIAFORMAT_KEY_AAC_PROFILE, encoder->aac_profile);
		AMediaFormat_setInt32(mediaFormat, AMEDIAFORMAT_KEY_BIT_RATE, encoder->audio_bitrate);
		AMediaFormat_setInt32(mediaFormat, AMEDIAFORMAT_KEY_CHANNEL_COUNT, encoder->channel_count);
		AMediaFormat_setInt32(mediaFormat, AMEDIAFORMAT_KEY_IS_ADTS, 0);
		uint8_t es[2] = {0x12, 0x12};
		AMediaFormat_setBuffer(mediaFormat, "csd-0", es, 2);
		encoder->frameIndex = 0;

		if (encoder->DEBUG) {
			MediaCodec_LOGI("[open]AMEDIAFORMAT_KEY_MIME : %s  sample_rate : %d  aac_profile : %d audio_bitrate : %d channel_count : %d",\
							encoder->MIMETYPE_AUDIO, encoder->sample_rate, encoder->aac_profile, encoder->audio_bitrate, encoder->channel_count);
		}
	}else{
		MediaCodec_LOGE("[open] error encode type...");
		return -3;
	}

	status = AMediaCodec_configure(encoder->codec, mediaFormat, NULL, NULL, 1);
	if(status){
		if (encoder->DEBUG) {
			MediaCodec_LOGE("[open]AMediaCodec_configure error");
		}
		return status;
	}
#if 0
	status = AMediaCodec_start(encoder->codec);
	if(status){
		if (encoder->DEBUG) {
			MediaCodec_LOGE("[open]AMediaCodec_start error");
		}
		return status;
	}

	status = AMediaCodec_flush(encoder->codec);
	if(status){
		if (encoder->DEBUG) {
			MediaCodec_LOGE("[open]AMediaCodec_flush error");
		}
		return status;
	}
#endif
	return status;
}

int mediacodec_encoder_start(MediaCodecEncoder* encoder)
{
	int status;

	status = AMediaCodec_start(encoder->codec);
	if(status){
		if (encoder->DEBUG) {
			MediaCodec_LOGE("[start]AMediaCodec_start error");
		}
		return status;
	}
	return status;
}

int mediacodec_encoder_close(MediaCodecEncoder* encoder) {
	if(encoder){
		if (encoder->DEBUG) {
			MediaCodec_LOGI("[close]");
		}

		int status = 0;

		if(encoder->codec){
			status = AMediaCodec_flush(encoder->codec);
			if(status){
				if (encoder->DEBUG) {
					MediaCodec_LOGE("[close]AMediaCodec_flush error");
				}
				return status;
			}
			status = AMediaCodec_stop(encoder->codec);
			if(status){
				if (encoder->DEBUG) {
					MediaCodec_LOGE("[close]AMediaCodec_stop error");
				}
				return status;
			}
			status = AMediaCodec_delete(encoder->codec);
			if(status){
				if (encoder->DEBUG) {
					MediaCodec_LOGE("[close]AMediaCodec_delete error");
				}
				return status;
			}
			encoder->codec = NULL;
		}
		if(encoder->sps){
			free(encoder->sps);
			encoder->sps = NULL;
		}
		if(encoder->pps){
			free(encoder->pps);
			encoder->pps = NULL;
		}
		if(encoder->phone_type){
			free(encoder->phone_type);
			encoder->phone_type = NULL;
		}
		if(encoder->hardware){
			free(encoder->hardware);
			encoder->hardware = NULL;
		}
		return status;
	}
	else{
		if (encoder->DEBUG) {
			MediaCodec_LOGE("[close]ERROR_SDK_ENCODER_IS_NULL");
		}
		return -1;
	}
}

int mediacodec_encoder_encode(MediaCodecEncoder* encoder, uint8_t* in, int offset, uint8_t* out, int length, int* error_code, AMediaCodecBufferInfo *bufferInfo) {
	if(encoder){
		if (encoder->DEBUG) {
			MediaCodec_LOGI("[encode]");
		}

		int pos = 0;
		*error_code = 0;

		if (out == NULL) {
			if (encoder->DEBUG) {
				if (encoder->DEBUG) {
					MediaCodec_LOGE("ERROR_CODE_OUT_BUF_NULL");
				}
			}
			return -2;//ERROR_CODE_OUT_BUF_NULL
		}

		ssize_t inputBufferIndex = AMediaCodec_dequeueInputBuffer(encoder->codec, -1);
		size_t inputBufferSize = 0;

		if (encoder->DEBUG) {
			MediaCodec_LOGI("inputBufferIndex : %ld", inputBufferIndex);
		}

		if (inputBufferIndex >= 0) {
			uint8_t* inputBuffer = AMediaCodec_getInputBuffer(encoder->codec, inputBufferIndex, &inputBufferSize);
			if(inputBuffer != NULL && inputBufferSize >= (unsigned long)length){
				if(0 == encoder->is_audio_type){
					if(encoder->hardware[0] == 'm' && encoder->hardware[1] == 't'){
						if(encoder->yuv_pixel_format == I420){
							memmove(inputBuffer, in, length);
						}
						else if(encoder->yuv_pixel_format == NV12){
							NV12toYUV420Planar(in, offset, inputBuffer, encoder->width, encoder->height);
						}
						else if(encoder->yuv_pixel_format == NV21){
							NV21toYUV420Planar(in, offset, inputBuffer, encoder->width, encoder->height);
						}
					}
					else{
						if(encoder->yuv_pixel_format == I420){
							I420toYUV420SemiPlanar(in, offset, inputBuffer, encoder->width, encoder->height);
						}
						else if(encoder->yuv_pixel_format == NV12){
							memmove(inputBuffer, in, length);
						}
						else if(encoder->yuv_pixel_format == NV21){
							swapNV12toNV21(in, offset, inputBuffer, encoder->width, encoder->height);
						}
					}
					AMediaCodec_queueInputBuffer(encoder->codec, inputBufferIndex, 0, length, mediacodec_encoder_computePresentationTime(encoder), 0);
				}else if(encoder->is_audio_type == 1){
					memmove(inputBuffer, in, length);
					AMediaCodec_queueInputBuffer(encoder->codec, inputBufferIndex, 0, length, mediacodec_audio_encoder_computePresentationTime(encoder), 0);
				}else{
					*error_code = -3;//ERROR_CODE_INPUT_BUFFER_FAILURE
				}
			}else{
				if (encoder->DEBUG) {
					MediaCodec_LOGE("ERROR_CODE_INPUT_BUFFER_FAILURE inputBufferSize/in.length : %ld/%d", inputBufferIndex, length);
				}
				*error_code = -3;//ERROR_CODE_INPUT_BUFFER_FAILURE
			}
		} else {
			if (encoder->DEBUG) {
				MediaCodec_LOGE("ERROR_CODE_INPUT_BUFFER_FAILURE inputBufferIndex : %ld", inputBufferIndex);
			}
			*error_code = -3;//ERROR_CODE_INPUT_BUFFER_FAILURE
		}

		/*AMediaCodecBufferInfo bufferInfo;*/
		ssize_t outputBufferIndex = outputBufferIndex = AMediaCodec_dequeueOutputBuffer(encoder->codec, bufferInfo, encoder->TIME_OUT);
		size_t outputBufferSize = 0;
		if (encoder->DEBUG) {
			MediaCodec_LOGI("outputBufferIndex : %ld",outputBufferIndex);
		}
		if(outputBufferIndex != AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
			if(outputBufferIndex <= -20000){
				if (encoder->DEBUG) {
					MediaCodec_LOGE("AMEDIA_DRM_ERROR_BASE");
				}
				*error_code = outputBufferIndex;
				return pos;
			}
			if(outputBufferIndex <= -10000){
				if (encoder->DEBUG) {
					MediaCodec_LOGE("AMEDIA_ERROR_BASE");
				}
				*error_code = outputBufferIndex;
				return pos;
			}

			if (outputBufferIndex == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED) {
				// outputBuffers = codec.getOutputBuffers();
			}
			else if (outputBufferIndex == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
				MediaCodec_LOGE("======AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED=======");
				/**
				 * <code>
				 *	mediaFormat : {
				 *		image-data=java.nio.HeapByteBuffer[pos=0 lim=104 cap=104],
				 *		mime=video/raw,
				 *		crop-top=0,
				 *		crop-right=703,
				 *		slice-height=576,
				 *		color-format=21,
				 *		height=576, width=704,
				 *		crop-bottom=575, crop-left=0,
				 *		hdr-static-info=java.nio.HeapByteBuffer[pos=0 lim=25 cap=25],
				 *		stride=704
				 *    }
				 *	</code>
				 */

				AMediaFormat* mediaFormat = AMediaCodec_getOutputFormat(encoder->codec);

				AMediaFormat_getString(mediaFormat, AMEDIAFORMAT_KEY_MIME, &(encoder->mime));
				if(0 == encoder->is_audio_type)//video
					AMediaFormat_getInt32(mediaFormat, AMEDIAFORMAT_KEY_COLOR_FORMAT, &(encoder->colorFormat));

				/*if (encoder->DEBUG) {*/
					MediaCodec_LOGI("mediaFormat : %s",AMediaFormat_toString(mediaFormat));
				/*}*/

				if(encoder->_format_changed_cb != NULL) {
					MediaCodec_LOGI("%s call _format_changed_cb \n", __func__);
					encoder->_format_changed_cb(mediaFormat);
				}else{
					MediaCodec_LOGI("%s _format_changed_cb NULL\n", __func__);
				}

			}else if (outputBufferIndex >= 0) {
				//if (encoder->DEBUG) {
				if(0 == encoder->is_audio_type)//video
				{

					MediaCodec_LOGI("video: bufferInfo.size=%d bufferInfo.offset=%d info.time[%lld]", bufferInfo->size, bufferInfo->offset, bufferInfo->presentationTimeUs);
				}else{
					MediaCodec_LOGI("audio: bufferInfo.size=%d bufferInfo.offset=%d info.time[%lld]", bufferInfo->size, bufferInfo->offset, bufferInfo->presentationTimeUs);
				}
				uint8_t* outputBuffer = AMediaCodec_getOutputBuffer(encoder->codec, outputBufferIndex, &outputBufferSize);
				if(outputBuffer){
					if(!strcmp("video/avc",encoder->MIMETYPE_VIDEO_AVC)){
						if (encoder->sps == NULL || encoder->pps == NULL) {//pps sps只有开始时第一个帧里有，保存起来后面用
							offset = 0;

							while (offset < bufferInfo->size && (encoder->sps == NULL || encoder->pps == NULL)) {
								while (outputBuffer[offset] == 0) {
									offset++;
								}

								offset++;
								int count = mediacodec_encoder_ffAvcFindStartcode(outputBuffer, offset, bufferInfo->size);

								int naluLength = count - offset;
								int type = outputBuffer[offset] & 0x1F;
								if (encoder->DEBUG) {
									MediaCodec_LOGI("count : %d offset : %d type : %d",count,offset,type);
								}
								if (type == 7) {
									encoder->sps = (uint8_t*)malloc(naluLength + encoder->startCode_length);
									encoder->sps_length = naluLength + encoder->startCode_length;
									memmove(encoder->sps, encoder->startCode, encoder->startCode_length);
									memmove(encoder->sps+encoder->startCode_length, outputBuffer+offset, naluLength);
								} else if (type == 8) {
									encoder->pps = (uint8_t*)malloc(naluLength + encoder->startCode_length);
									encoder->pps_length = naluLength + encoder->startCode_length;
									memmove(encoder->pps, encoder->startCode, encoder->startCode_length);
									memmove(encoder->pps+encoder->startCode_length, outputBuffer+offset, naluLength);
								} else if (type == 5) {
									encoder->firstI = 1;
								}
								offset += naluLength;
							}
						}
						else{
							if (!encoder->firstI) {
								offset = 0;
								while (offset < bufferInfo->size && !encoder->firstI) {
									while (outputBuffer[offset] == 0) {
										offset++;

										if (offset >= bufferInfo->size) {
											break;
										}
									}

									if (offset >= bufferInfo->size) {
										break;
									}

									offset++;
									int count = mediacodec_encoder_ffAvcFindStartcode(outputBuffer, offset, bufferInfo->size);

									if (offset >= bufferInfo->size) {
										break;
									}

									int naluLength = count - offset;
									int type = outputBuffer[offset] & 0x1F;
									if (encoder->DEBUG) {
										MediaCodec_LOGI("count : %d offset : %d type : %d",count,offset,type);
									}
									if (type == 5) {
										encoder->firstI = 1;
									}
									offset += naluLength;
								}
							}
						}

						// key frame 编码器生成关键帧时只有 00 00 00 01 65
						// 没有pps encoder->sps,要加上
						if ((outputBuffer[encoder->startCode_length] & 0x1f) == 5) {
							if(encoder->sps){
								memmove(out+pos, encoder->sps, encoder->sps_length);
								pos += encoder->sps_length;
							}
							if(encoder->pps){
								memmove(out+pos, encoder->pps, encoder->pps_length);
								pos += encoder->pps_length;
							}
							memmove(out+pos, outputBuffer, bufferInfo->size);
							pos += bufferInfo->size;
						}
						else
						{
							memmove(out+pos, outputBuffer, bufferInfo->size);
							pos += bufferInfo->size;
						}

						if (!encoder->firstI) {
							pos = 0;
						}
					}else if(!strcmp("video/hevc",encoder->MIMETYPE_VIDEO_AVC)){
						memmove(out+pos, outputBuffer, bufferInfo->size);
						pos += bufferInfo->size;
					}else{// "audio/mp4a-latm"
						memmove(out+pos, outputBuffer, bufferInfo->size);
						pos += bufferInfo->size;
					}
				}
				AMediaCodec_releaseOutputBuffer(encoder->codec, outputBufferIndex, 0);
			}
		}
		return pos;
	}else{
		MediaCodec_LOGE("[encode]ERROR_SDK_ENCODER_IS_NULL");
		*error_code = -4;
		return 0;
	}
}

int mediacodec_encoder_getConfig_int(MediaCodecEncoder* encoder, char* key) {
	if (!strcmp(AMEDIAFORMAT_KEY_WIDTH, key)) {
		return encoder->width;
	}
	else if (!strcmp(AMEDIAFORMAT_KEY_HEIGHT, key)) {
		return encoder->height;
	}
	else if (!strcmp(AMEDIAFORMAT_KEY_COLOR_FORMAT, key)) {
		return encoder->colorFormat;
	}
	else if (!strcmp("timeout", key)) {
		return encoder->TIME_OUT;
	}
	else if (!strcmp("max-timeout", key)) {
		return encoder->MAX_TIME_OUT;
	}
	return -1;
}

int mediacodec_encoder_setConfig_int(MediaCodecEncoder* encoder, char* key, int value) {
	if (!strcmp("timeout", key)) {
		encoder->TIME_OUT = value;
		return 0;
	}
	else{
		return -1;
	}
}

extern int64_t nanoTime;
uint64_t mediacodec_encoder_computePresentationTime(MediaCodecEncoder* encoder) {
	return (systemnanotime() - nanoTime) / 1000;
	encoder->frameIndex++;
}

uint64_t mediacodec_audio_encoder_computePresentationTime(MediaCodecEncoder* encoder) {
	return (systemnanotime() - nanoTime) / 1000;
	encoder->frameIndex++;
}

int mediacodec_encoder_ffAvcFindStartcodeInternal(uint8_t* data, int offset, int end) {
	int a = offset + 4 - (offset & 3);

	for (end -= 3; offset < a && offset < end; offset++) {
		if (data[offset] == 0 && data[offset + 1] == 0 && data[offset + 2] == 1){
			return offset;
		}
	}

	for (end -= 3; offset < end; offset += 4) {
		int x = ((data[offset] << 8 | data[offset + 1]) << 8 | data[offset + 2]) << 8 | data[offset + 3];
//			System.out.println(Integer.toHexString(x));
		// if ((x - 0x01000100) & (~x) & 0x80008000) // little endian
		// if ((x - 0x00010001) & (~x) & 0x00800080) // big endian
		if (((x - 0x01010101) & (~x) & 0x80808080) != 0) { // generic
			if (data[offset + 1] == 0) {
				if (data[offset] == 0 && data[offset + 2] == 1)
					return offset;
				if (data[offset + 2] == 0 && data[offset + 3] == 1)
					return offset + 1;
			}
			if (data[offset + 3] == 0) {
				if (data[offset + 2] == 0 && data[offset + 4] == 1)
					return offset + 2;
				if (data[offset + 4] == 0 && data[offset + 5] == 1)
					return offset + 3;
			}
		}
	}

	for (end += 3; offset < end; offset++) {
		if (data[offset] == 0 && data[offset + 1] == 0 && data[offset + 2] == 1){
			return offset;
		}
	}

	return end + 3;
}

int mediacodec_encoder_ffAvcFindStartcode(uint8_t* data, int offset, int end) {
	int out = mediacodec_encoder_ffAvcFindStartcodeInternal(data, offset, end);
	if (offset < out && out < end && data[out - 1] == 0){
		out--;
	}
	return out;
}

/**
 * 最简单的基于FFmpeg的音频播放器 2
 * Simplest FFmpeg Audio Player 2
 *
 * 雷霄骅 Lei Xiaohua
 * leixiaohua1020@126.com
 * 中国传媒大学/数字电视技术
 * Communication University of China / Digital TV Technology
 * http://blog.csdn.net/leixiaohua1020
 *
 * 本程序实现了音频的解码和播放。
 * 是最简单的FFmpeg音频解码方面的教程。
 * 通过学习本例子可以了解FFmpeg的解码流程。
 *
 * 该版本使用SDL 2.0替换了第一个版本中的SDL 1.0。
 * 注意：SDL 2.0中音频解码的API并无变化。唯一变化的地方在于
 * 其回调函数的中的Audio Buffer并没有完全初始化，需要手动初始化。
 * 本例子中即SDL_memset(stream, 0, len);
 *
 * This software decode and play audio streams.
 * Suitable for beginner of FFmpeg.
 *
 * This version use SDL 2.0 instead of SDL 1.2 in version 1
 * Note:The good news for audio is that, with one exception,
 * it's entirely backwards compatible with 1.2.
 * That one really important exception: The audio callback
 * does NOT start with a fully initialized buffer anymore.
 * You must fully write to the buffer in all cases. In this
 * example it is SDL_memset(stream, 0, len);
 *
 * Version 2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <mutex>
#define __STDC_CONSTANT_MACROS

 //#ifdef _WIN32
 // //Windows
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswresample/swresample.h"
#include "SDL2/SDL.h"
#include "libavutil/frame.h"
#include "sonic.h"
};
//#else
// //Linux...
//#ifdef __cplusplus
//extern "C"
//{
//#endif
//#include <libavcodec/avcodec.h>
//#include <libavformat/avformat.h>
//#include <libswresample/swresample.h>
//#include <SDL2/SDL.h>
//#include <libavutil/frame.h>
//#include <sonic.h>
//#ifdef __cplusplus
//};
//#endif
//#endif

#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000
#define BUFFER_SIZE 4608
//下面这四个结构体是为了分析wav头的
typedef struct {
	uint32_t magic;      /* 'RIFF' */
	uint32_t length;     /* filelen */
	uint32_t type;       /* 'WAVE' */
} WaveHeader;

typedef struct {
	uint16_t format;       /* see WAV_FMT_* */
	uint16_t channels;
	uint32_t sample_fq;      /* frequence of sample */
	uint32_t byte_p_sec;
	uint16_t byte_p_spl;   /* samplesize; 1 or 2 bytes */
	uint16_t bit_p_spl;    /* 8, 12 or 16 bit */
} WaveFmtBody;

typedef struct {
	uint32_t type;        /* 'data' */
	uint32_t length;      /* samplecount */
} WaveChunkHeader;

#define COMPOSE_ID(a,b,c,d) ((a) | ((b)<<8) | ((c)<<16) | ((d)<<24))
#define WAV_RIFF COMPOSE_ID('R','I','F','F')
#define WAV_WAVE COMPOSE_ID('W','A','V','E')
#define WAV_FMT COMPOSE_ID('f','m','t',' ')
#define WAV_DATA COMPOSE_ID('d','a','t','a')
int insert_wave_header(FILE* fp, long data_len)
{
	int len;
	WaveHeader* header;
	WaveChunkHeader* chunk;
	WaveFmtBody* body;

	fseek(fp, 0, SEEK_SET);        //写到wav文件的开始处

	len = sizeof(WaveHeader) + sizeof(WaveFmtBody) + sizeof(WaveChunkHeader) * 2;
	char* buf = (char*)malloc(len);
	header = (WaveHeader*)buf;
	header->magic = WAV_RIFF;
	header->length = data_len + sizeof(WaveFmtBody) + sizeof(WaveChunkHeader) * 2 + 4;
	header->type = WAV_WAVE;

	chunk = (WaveChunkHeader*)(buf + sizeof(WaveHeader));
	chunk->type = WAV_FMT;
	chunk->length = 16;

	body = (WaveFmtBody*)(buf + sizeof(WaveHeader) + sizeof(WaveChunkHeader));
	body->format = (uint16_t)0x0001;      //编码方式为pcm
	body->channels = (uint16_t)0x02;      //声道数为2
	body->sample_fq = 44100;             //采样频率为44.1k 
	body->byte_p_sec = 176400;           //每秒所需字节数 44100*2*2=采样频率*声道*采样位数 
	body->byte_p_spl = (uint16_t)0x4;     //对齐无意义
	body->bit_p_spl = (uint16_t)16;       //采样位数16bit=2Byte


	chunk = (WaveChunkHeader*)(buf + sizeof(WaveHeader) + sizeof(WaveChunkHeader) + sizeof(WaveFmtBody));
	chunk->type = WAV_DATA;
	chunk->length = data_len;

	fwrite(buf, 1, len, fp);
	free(buf);
	return 0;
}

typedef struct {
	int videoindex;
	int sndindex;
	AVFormatContext* pFormatCtx;
	AVCodecContext* sndCodecCtx;
	AVCodec* sndCodec;
	SwrContext* swr_ctx;
	DECLARE_ALIGNED(16, uint8_t, audio_buf)[AVCODEC_MAX_AUDIO_FRAME_SIZE * 4];
}AudioState;


#define MAX_AUDIO_FRAME_SIZE 192000 // 1 second of 48khz 32bit audio

//Output PCM
#define OUTPUT_PCM 1
//Use SDL
#define USE_SDL 1

//Buffer:
//|-----------|-------------|
//chunk-------pos---len-----|
static  Uint8* audio_chunk;
static  Uint32  audio_len;
static  Uint8* audio_pos;

/* The audio function callback takes the following parameters:
 * stream: A pointer to the audio buffer to be filled
 * len: The length (in bytes) of the audio buffer
*/
bool audio_play_status = true;
void  fill_audio(void* udata, Uint8* stream, int len) {
	//SDL 2.0
	//SDL_memset(stream, 0, len);
	//if (audio_len == 0)
	//	return;

	//len = (len > audio_len ? audio_len : len);	/*  Mix  as  much  data  as  possible  */

	//SDL_MixAudio(stream, audio_pos, len, SDL_MIX_MAXVOLUME);
	//audio_pos += len;
	//audio_len -= len;
	SDL_memset(stream, 0, len);
	while (len > 0)
	{
		if (!audio_play_status)  return;
		if (audio_len <= 0) {
			/*audio_len = 1024;
			memset(stream, 0, audio_len);*/
			continue;
		}
		//continue;
		int temp = (len > audio_len ? audio_len : len);
		SDL_MixAudio(stream, audio_pos, temp, SDL_MIX_MAXVOLUME);
		audio_pos += temp;
		audio_len -= temp;
		stream += temp;
		len -= temp;
	}
}
//-----------------

namespace Speed {

	void readSamples(unsigned char* bytes, short* buff, int maxSamples, int channel_num) {
		int bytePos = 0;
		short sample;
		for (int i = 0; i < maxSamples * channel_num; i++) {
			sample = bytes[bytePos++];
			sample += (unsigned int)bytes[bytePos++] << 8;
			*buff++ = sample;
			//printf("%d\n",sample);
		}
	}

	void writeSamples(unsigned char* bytes, short* buff, int maxSamples, int channel_num) {
		int bytePos = 0;
		short sample;
		for (int i = 0; i < maxSamples * channel_num; i++) {
			sample = buff[i];
			bytes[bytePos++] = sample;
			bytes[bytePos++] = sample >> 8;
			//printf("%d\n", sample);
		}
	}
}

AVFormatContext* initAVFormatContext(char* url) {
	AVFormatContext* pFormatCtx = avformat_alloc_context();
	//Open
	if (avformat_open_input(&pFormatCtx, url, NULL, NULL) != 0) {
		printf("Couldn't open input stream.\n");
		exit(1);
	}
	// Retrieve stream information
	if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
		printf("# Couldn't find stream information.\n");
		exit(1);
	}
	// Dump valid information onto standard error
	av_dump_format(pFormatCtx, 0, url, false);
	return pFormatCtx;
}

int findStreamAndCodec(AVFormatContext* pFormatCtx, const AVCodec** pCodec) {
	int audioStream = av_find_best_stream(pFormatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, pCodec, 0);
	if (audioStream == -1) {
		printf("AudioStream not fount.\n");
		exit(1);
	}
	if (pCodec == NULL) {
		printf("Codec not found.\n");
		exit(1);
	}
	return audioStream;
}

AVCodecContext* initAVCodecContext(AVFormatContext* pFormatCtx, const AVCodec* pCodec, int streamIndex) {
	AVCodecContext* pCodecCtx = avcodec_alloc_context3(NULL);
	avcodec_parameters_to_context(pCodecCtx, pFormatCtx->streams[streamIndex]->codecpar);
	// Open codec
	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
		printf("Could not open codec.\n");
		exit(1);
	}
	return pCodecCtx;
}

typedef struct AudioParams
{
	uint64_t channel_layout;
	int samples_per_frame;
	AVSampleFormat sample_format;
	int sample_rate;
	int channels;
	int sample_size;
	int buffer_size;
	uint8_t* buffer;
}AudioParams;

AudioParams* initOutAudioParams(AVCodecContext* pCodecCtx) {
	//Out Audio Param
	uint64_t out_channel_layout = AV_CH_LAYOUT_STEREO;
	//nb_samples: AAC-1024 MP3-1152
	int out_nb_samples = pCodecCtx->frame_size;

	AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;
	int out_sample_rate = 44100;
	//int out_sample_rate = pCodecCtx->sample_rate;
	int out_channels = av_get_channel_layout_nb_channels(out_channel_layout);
	//Out Buffer Size
	int out_buffer_size = av_samples_get_buffer_size(NULL, out_channels, out_nb_samples, out_sample_fmt, 1);

	uint8_t* out_buffer = (uint8_t*)av_malloc(MAX_AUDIO_FRAME_SIZE * 2);

	AudioParams* params = (AudioParams*)av_malloc(sizeof(AudioParams));
	params->channel_layout = out_channel_layout;
	params->samples_per_frame = out_nb_samples;
	params->sample_format = out_sample_fmt;
	params->sample_rate = out_sample_rate;
	params->channels = out_channels;
	params->sample_size = av_get_bytes_per_sample(out_sample_fmt);
	params->buffer_size = out_buffer_size;
	params->buffer = out_buffer;
	return params;
}

void free(AudioParams* audioParams) {
	uint8_t* buffer = audioParams->buffer;
	av_free(audioParams);
	av_free(buffer);
}

typedef struct Frame {
	Frame* next = nullptr;
	uint8_t* data;
	int size;
} Frame;

int MAX_FRAME_CNT = 6;
typedef struct FrameQueue {
	Frame* head = nullptr;
	Frame* tail = nullptr;
	std::atomic<int> size = 0;
} FrameQueue;

std::mutex mutex;
std::condition_variable notFull;
std::condition_variable notEmpty;
FrameQueue queue;

Frame* framePool = nullptr;
std::atomic<int> poolSize = 0;
std::atomic<int> frameSize = 0;
std::mutex poolMutex;
std::condition_variable notPoolFull;

Frame* allocateFrame() {
	std::unique_lock<std::mutex> lock(poolMutex);
	while (poolSize == MAX_AUDIO_FRAME_SIZE) {
		notPoolFull.wait(lock);
	}
	Frame* frame = nullptr;
	if (frameSize < MAX_AUDIO_FRAME_SIZE && poolSize == 0) {
		uint8_t* data = (uint8_t*)malloc(BUFFER_SIZE);
		frame = (Frame*)malloc(sizeof(Frame));
		frame->data = data;
		frameSize++;
	}
	else {
		frame = framePool;
		framePool = frame->next;
		frame->next = nullptr;
		poolSize--;
	}
	return frame;
}

void recycleFrame(Frame* frame) {
	std::unique_lock<std::mutex> lock(poolMutex);
	frame->next = framePool;
	frame->size = 0;
	framePool = frame;
	poolSize++;
	if (poolSize == 1) notPoolFull.notify_one();
}

void enqueue(Frame* frame) {
	if (queue.head == nullptr) {
		queue.head = frame;
	}
	else {
		queue.tail->next = frame;
	}
	queue.tail = frame;
	queue.size++;
}

Frame* dequeue() {
	Frame* h = queue.head;
	Frame* first = h->next;
	h->next = nullptr;
	queue.head = first;
	queue.size--;
	if (queue.size == 0) {
		queue.tail = nullptr;
		queue.head = nullptr;
	}
	return h;
}

Frame* getAudioData() {
	std::unique_lock<std::mutex> lock(mutex);
	while (queue.size == 0) {
		notEmpty.wait(lock);
	}
	Frame* frame = dequeue();
	if (queue.size == MAX_FRAME_CNT - 1) {
		notFull.notify_one();
	}
	return frame;
}

void putAudioData(Frame* frame) {
	std::unique_lock<std::mutex> lock(mutex);
	while (queue.size == MAX_FRAME_CNT) {
		notFull.wait(lock);
	}
	enqueue(frame);
	if (queue.size == 1) {
		notEmpty.notify_one();
	}
}

void audioSpeed(AudioParams* outParams, FILE* pFile, sonicStream stream, short* sampleInBuffer, short* sampleOutBuffer, int samples_num) {
	int samplesWritten;
	if (samples_num == 0) {
		sonicFlushStream(stream);
	}
	else {
		//printf("samples_num : %d\n", samples_num);
		Speed::readSamples(outParams->buffer, sampleInBuffer, samples_num, 2);
		sonicWriteShortToStream(stream, sampleInBuffer, samples_num);
	}
	do {
		samplesWritten = sonicReadShortFromStream(stream, sampleOutBuffer, BUFFER_SIZE / outParams->channels / 2);
		//printf("%d\n", samplesWritten);
		if (samplesWritten > 0) {
			//memset(outBuf, 0, BUFFER_SIZE);
			int size = samplesWritten * outParams->channels * outParams->sample_size;
			Frame* frame = allocateFrame();
			frame->size = size;
			Speed::writeSamples(frame->data, sampleOutBuffer, samplesWritten, outParams->channels);
			//fwrite(outBuf, 1, samplesWritten * 2 * 2, pOutFile);
#if OUTPUT_PCM
					//Write PCM
			fwrite(frame->data, 1, size, pFile);
#endif

#if USE_SDL
			putAudioData(frame);
#endif
		}
	} while (samplesWritten > 0);
}

void decodeAudio(AVFormatContext* pFormatCtx, AVCodecContext* pCodecCtx, AVPacket* packet, AVFrame* pFrame,
	int streamIndex, SwrContext* au_convert_ctx, AudioParams* outParams, FILE* pFile,
	sonicStream stream, short* sampleInBuffer, short* sampleOutBuffer) {

	//ret = avcodec_decode_audio4(pCodecCtx, pFrame, &got_picture, packet);
	int ret = avcodec_send_packet(pCodecCtx, packet);
	if (ret < 0) {
		printf("Error in decoding audio frame.\n");
		exit(1);
	}
	while (ret >= 0) {
		ret = avcodec_receive_frame(pCodecCtx, pFrame);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
			if (packet == nullptr) {
				audioSpeed(outParams, pFile, stream, sampleInBuffer, sampleOutBuffer, 0);
			}
			return;
		}
		else if (ret < 0) {
			printf("Error in receive audio frame.\n");
			exit(1);
		}

		//if (got_picture > 0) {
		int samples_num = swr_convert(au_convert_ctx, &outParams->buffer, outParams->sample_rate * 2, (const uint8_t**)pFrame->data, pFrame->nb_samples);
		//如果out_sample_rate不和原音频保持一致，那么av_samples_get_buffer_size获取的不是精准的，会导致最后输出的pcm文件有杂音
		//https://blog.csdn.net/z345436330/article/details/90742499?spm=1001.2101.3001.6650.2&utm_medium=distribute.pc_relevant.none-task-blog-2%7Edefault%7ECTRLIST%7Edefault-2.nonecase&depth_1-utm_source=distribute.pc_relevant.none-task-blog-2%7Edefault%7ECTRLIST%7Edefault-2.nonecase
		//int convert_buffer_size = samples_num * outParams->channels * outParams->sample_size;
		audioSpeed(outParams, pFile, stream, sampleInBuffer, sampleOutBuffer, samples_num);
		//printf("len : %d,out_buffer_size : %d\n", len, out_buffer_size);
		//index++;
		//}
	}

}

void decodeStream(AVFormatContext* pFormatCtx, AVCodecContext* pCodecCtx, AVPacket* packet, AVFrame* pFrame,
	int streamIndex, SwrContext* au_convert_ctx, AudioParams* outParams, FILE* pFile) {

	int index = 0;
	sonicStream stream = sonicCreateStream(outParams->sample_rate, outParams->channels);
	float speed = 2;
	sonicSetSpeed(stream, speed);
	//unsigned char speed_out_buffer[BUFFER_SIZE] = { 0 };
	short sampleInBuffer[BUFFER_SIZE / 2] = { 0 }, sampleOutBuffer[BUFFER_SIZE / 2] = { 0 };
	while (av_read_frame(pFormatCtx, packet) >= 0) {
		if (packet->stream_index == streamIndex) {
			decodeAudio(pFormatCtx, pCodecCtx, packet, pFrame,
				streamIndex, au_convert_ctx, outParams, pFile,
				stream, sampleInBuffer, sampleOutBuffer);
		}
		//av_free_packet(packet);
	}

	decodeAudio(pFormatCtx, pCodecCtx, nullptr, pFrame,
		streamIndex, au_convert_ctx, outParams, pFile,
		stream, sampleInBuffer, sampleOutBuffer);


#if USE_SDL
	Frame* end = allocateFrame();
	end->data = nullptr;
	putAudioData(end);
	/*SDL_Event event;
	event.type = FF_QUIT_EVENT;
	SDL_PushEvent(&event);*/
#endif

#if OUTPUT_PCM
	fclose(pFile);
#endif
	//av_free(out_buffer);
	swr_free(&au_convert_ctx);
	free(outParams);
	av_packet_free(&packet);
	av_frame_free(&pFrame);
	//avcodec_close(pCodecCtx);
	avcodec_free_context(&pCodecCtx);
	avformat_close_input(&pFormatCtx);
	sonicDestroyStream(stream);
}

int main(int argc, char* argv[])
{
	AVFormatContext* pFormatCtx;
	int				i, audioStream = -1;
	AVCodecContext* pCodecCtx;
	const AVCodec* pCodec;
	AVPacket* packet;
	//uint8_t* out_buffer;
	AVFrame* pFrame;
	SDL_AudioSpec wanted_spec;
	int ret;
	uint32_t len = 0;
	int got_picture;
	int64_t in_channel_layout;
	struct SwrContext* au_convert_ctx;
	AudioParams* outParams;

	FILE* pFile = NULL;
	char url[] = "../../../simplest_ffmpeg_audio_player_speed/xiaoqingge.mp3";


	pFormatCtx = initAVFormatContext(url);

	audioStream = findStreamAndCodec(pFormatCtx, &pCodec);

	pCodecCtx = initAVCodecContext(pFormatCtx, pCodec, audioStream);


#if OUTPUT_PCM
	pFile = fopen("output.pcm", "wb");
#endif

	//packet = (AVPacket*)av_malloc(sizeof(AVPacket));
	//av_init_packet(packet);
	packet = av_packet_alloc();
	pFrame = av_frame_alloc();

	outParams = initOutAudioParams(pCodecCtx);


	//FIX:Some Codec's Context Information is missing
	in_channel_layout = av_get_default_channel_layout(pCodecCtx->channels);
	//Swr

	au_convert_ctx = swr_alloc();
	au_convert_ctx = swr_alloc_set_opts(au_convert_ctx,
		outParams->channel_layout,
		outParams->sample_format,
		outParams->sample_rate,
		in_channel_layout, pCodecCtx->sample_fmt, pCodecCtx->sample_rate, 0, NULL);
	swr_init(au_convert_ctx);


	//pthread_t thread;
//pthread_create(&thread,NULL,)
	std::thread decodeThread(decodeStream, pFormatCtx, pCodecCtx, packet, pFrame,
		audioStream, au_convert_ctx, outParams, pFile);

#if USE_SDL
	decodeThread.detach();
#else
	decodeThread.join();
#endif

	//int frame_count = pCodecCtx->frame_number;



#if USE_SDL
	//Init
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
		printf("Could not initialize SDL - %s\n", SDL_GetError());
		return -1;
	}
	//SDL_AudioSpec
	wanted_spec.freq = outParams->sample_rate;
	wanted_spec.format = AUDIO_S16SYS;
	wanted_spec.channels = outParams->channels;
	wanted_spec.silence = 0;
	wanted_spec.samples = outParams->samples_per_frame;
	wanted_spec.callback = fill_audio;
	//wanted_spec.userdata = pCodecCtx;

	if (SDL_OpenAudio(&wanted_spec, NULL) < 0) {
		printf("can't open audio.\n");
		return -1;
	}

	//Play
	SDL_PauseAudio(0);
	uint8_t* sdlData = (uint8_t*)malloc(BUFFER_SIZE);
	while (true)
	{

		while (audio_len > 0)//Wait until finish
		//SDL_Delay(1);
			;

		//Set audio buffer (PCM data)
		Frame* frame = getAudioData();
		if (frame->data == nullptr) {
			printf("queue frame cnt: %d\n", queue.size.load());
			break;
		}
		else
		{
			memcpy(sdlData, frame->data, frame->size);
			audio_chunk = sdlData;
			//Audio buffer length
			audio_len = frame->size;
			audio_pos = audio_chunk;
		}
		recycleFrame(frame);
	}
	audio_play_status = false;
	SDL_CloseAudio();
	SDL_Quit();
#endif

	return 0;
}
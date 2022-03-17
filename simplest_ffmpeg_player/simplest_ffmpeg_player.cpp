/**
 * ��򵥵Ļ���FFmpeg����Ƶ������ 2
 * Simplest FFmpeg Player 2
 *
 * ������ Lei Xiaohua
 * leixiaohua1020@126.com
 * �й���ý��ѧ/���ֵ��Ӽ���
 * Communication University of China / Digital TV Technology
 * http://blog.csdn.net/leixiaohua1020
 *
 * ��2��ʹ��SDL2.0ȡ���˵�һ���е�SDL1.2
 * Version 2 use SDL 2.0 instead of SDL 1.2 in version 1.
 *
 * ������ʵ������Ƶ�ļ��Ľ������ʾ(֧��HEVC��H.264��MPEG2��)��
 * ����򵥵�FFmpeg��Ƶ���뷽��Ľ̡̳�
 * ͨ��ѧϰ�����ӿ����˽�FFmpeg�Ľ������̡�
 * This software is a simplest video player based on FFmpeg.
 * Suitable for beginner of FFmpeg.
 *
 */



#include <stdio.h>

#define __STDC_CONSTANT_MACROS

#ifdef _WIN32
 //Windows
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "libavutil/error.h"
#include "SDL2/SDL.h"
};
#else
 //Linux...
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <SDL2/SDL.h>
#include <libavutil/imgutils.h>
#ifdef __cplusplus
};
#endif
#endif

//Output YUV420P data as a file 
#define OUTPUT_YUV420P 1

int main(int argc, char* argv[])
{
	AVFormatContext* pFormatCtx;
	int				i, videoindex;
	//ffmpeg4.4��ʹ��AVCodecParameters����AVCodecContext
	AVCodecContext* pCodecCtx;
	//AVCodecParameters* pCodecCtx;
	const AVCodec* pCodec = nullptr;
	AVFrame* pFrame, * pFrameYUV;
	unsigned char* out_buffer;
	AVPacket* packet;
	int y_size;
	int ret, got_picture;
	struct SwsContext* img_convert_ctx;

	char filepath[] = "../../../simplest_ffmpeg_player/trailer.mp4";
	//SDL---------------------------
	int screen_w = 0, screen_h = 0;
	SDL_Window* screen;
	SDL_Renderer* sdlRenderer;
	SDL_Texture* sdlTexture;
	SDL_Rect sdlRect;

	FILE* fp_yuv;

	//ffmpeg 4.0���Ժ󣬲���Ҫ�ٵ���
	// https://github.com/leandromoreira/ffmpeg-libav-tutorial/issues/29
	//av_register_all();
	avformat_network_init();
	//AVFormatContext����������ṹ�壺AVInputFormat��AVOutputFormat��AVIOContext
	pFormatCtx = avformat_alloc_context();
	pCodec = avcodec_find_decoder(AV_CODEC_ID_H264);
	if (avformat_open_input(&pFormatCtx, filepath, NULL, NULL) != 0) {
		printf("Couldn't open input stream.\n");
		return -1;
	}
	//����AVFormatContext�е�AVStream�ṹ���е��ֶ�
	//FFMEPG����avformat_find_stream_info����:https://blog.csdn.net/H514434485/article/details/77802382
	if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
		printf("Couldn't find stream information.\n");
		return -1;
	}
	videoindex = -1;
	
	//for (i = 0; i < pFormatCtx->nb_streams; i++)
	//	//AVStream�е��ֶ�codec��ffmpeg4.4���Ƴ�
	//	//if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
	//	if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
	//		videoindex = i;
	//		break;
	//	}
	//if (videoindex == -1) {
	//	printf("Didn't find a video stream.\n");
	//	return -1;
	//}

	////pCodecCtx = pFormatCtx->streams[videoindex]->codec;
	////pCodec = avcodec_find_decoder(pCodecCtx->codec_id);

	////��AVCodecParameters����AVCodecContext:https://blog.csdn.net/luotuo44/article/details/54981809
	//pCodec = avcodec_find_decoder(pFormatCtx->streams[videoindex]->codecpar->codec_id);

	//��av_find_best_stream�������ע�͵��Ĵ��룬��Ѱ��stream��index�ͳ�ʼ����Ӧ��AVCodec
	//ffmpegԴ�����2-av_find_best_stream:https://www.jianshu.com/p/60ab7f4d572a
	videoindex = av_find_best_stream(pFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &pCodec, 0);

	if (pCodec == NULL) {
		printf("Codec not found.\n");
		return -1;
	}
	//��Ҫʹ��avcodec_free_context�ͷ�
	pCodecCtx = avcodec_alloc_context3(pCodec);
	if (!pCodecCtx) {
		printf("Could not allocate video codec context\n");
		return -1;
	}
	//��ʵ��codecpar�����˴󲿷ֽ�������ص���Ϣ��������ֱ�Ӵ�AVCodecParameters���Ƶ�AVCodecContext
	//�����쳣��Picture size 0x0 is invalid
	avcodec_parameters_to_context(pCodecCtx, pFormatCtx->streams[videoindex]->codecpar);
	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
		printf("Could not open codec.\n");
		return -1;
	}

	pFrame = av_frame_alloc();
	pFrameYUV = av_frame_alloc();
	//TODO��align��1���ʹ�32������������
	out_buffer = (unsigned char*)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 32));
	//�������ݵ�pFrameYUV::data��Ҫ�ֶ���ʼ����
	av_image_fill_arrays(pFrameYUV->data, pFrameYUV->linesize, out_buffer,
		AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 1);

	//packet = (AVPacket*)av_malloc(sizeof(AVPacket));
	packet = av_packet_alloc();
	//Output Info-----------------------------
	//printf("--------------- File Information ----------------\n");
	av_dump_format(pFormatCtx, 0, filepath, 0);
	//printf("-------------------------------------------------\n");
	img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height, pCodecCtx->pix_fmt,
		pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);

#if OUTPUT_YUV420P 
	//w ��ֻд�ļ�
	//w + �򿪿ɶ�д�ļ�
	//w+�Դ��ı���ʽ��д����wb+���Զ����Ʒ�ʽ���ж�д��
	fp_yuv = fopen("output.yuv", "wb+");
#endif  

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
		printf("Could not initialize SDL - %s\n", SDL_GetError());
		return -1;
	}

	screen_w = pCodecCtx->width;
	screen_h = pCodecCtx->height;
	//SDL 2.0 Support for multiple windows
	screen = SDL_CreateWindow("Simplest ffmpeg player's Window", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		screen_w, screen_h,
		SDL_WINDOW_OPENGL);

	if (!screen) {
		printf("SDL: could not create window - exiting:%s\n", SDL_GetError());
		return -1;
	}

	sdlRenderer = SDL_CreateRenderer(screen, -1, 0);
	//IYUV: Y + U + V  (3 planes)
	//YV12: Y + V + U  (3 planes)
	sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING, pCodecCtx->width, pCodecCtx->height);

	sdlRect.x = 0;
	sdlRect.y = 0;
	sdlRect.w = screen_w;
	sdlRect.h = screen_h;

	//SDL End----------------------
	//��ȡ������һ֡����Ƶ�����߶�֡����Ƶ������䵽packet�С�
	//��h264����av_read_frame�����и�NALU���ؼ������ǣ�
	// av_read_frame(demux.c)->read_frame_internal->parse_packet->av_parser_parse2->parser_parse
	// (��h264_parse(h264_parse.c))
	//ffmpeg Դ����򵥷��� �� av_read_frame():https://blog.csdn.net/leixiaohua1020/article/details/12678577
	int frameCnt = 0;
	while (av_read_frame(pFormatCtx, packet) >= 0) {
		if (packet->stream_index == videoindex) {
			//ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);
			//��packet�����ݣ�������AVCodecContext��ȥ
			//��h264����avcodec_send_packet���ǽ���NALU���ؼ������ǣ�
			//avcodec_send_packet(decode.c)->decode_receive_frame_internal->decode_simple_receive_frame
			//->decode_simple_internal->decode(���룺ret = avctx->codec->decode(avctx, frame, &got_frame, pkt);)
			//(���codec��h264�ģ���ôdecode����ص�������������h264dec.c��ff_h264_decoder�����ʼ���ġ�
			//Ҳ����h264_decode_frame����)
			ret = avcodec_send_packet(pCodecCtx, packet);
			if (ret < 0) {
				printf("Decode Error.\n");
				return -1;
			}
			//packet����ܰ�����֡����ö�ε���avcodec_receive_frame��
			//��AVCodecContext������ݣ�������AVFrame��ȥ
			ret = avcodec_receive_frame(pCodecCtx, pFrame);
			if (ret == AVERROR(EAGAIN)) {
				printf("Receive Error: AVERROR(EAGAIN)\n");
				continue;
			}
			if (ret < 0) {
				printf("Receive Error: %d\n",ret);
			}
			frameCnt++;
			//if (got_picture) {
			sws_scale(img_convert_ctx, (const unsigned char* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height,
				pFrameYUV->data, pFrameYUV->linesize);

#if OUTPUT_YUV420P
			y_size = pCodecCtx->width * pCodecCtx->height;
			fwrite(pFrameYUV->data[0], 1, y_size, fp_yuv);    //Y 
			fwrite(pFrameYUV->data[1], 1, y_size / 4, fp_yuv);  //U
			fwrite(pFrameYUV->data[2], 1, y_size / 4, fp_yuv);  //V
#endif
				//SDL---------------------------
#if 0
			SDL_UpdateTexture(sdlTexture, NULL, pFrameYUV->data[0], pFrameYUV->linesize[0]);
#else
			SDL_UpdateYUVTexture(sdlTexture, &sdlRect,
				pFrameYUV->data[0], pFrameYUV->linesize[0],
				pFrameYUV->data[1], pFrameYUV->linesize[1],
				pFrameYUV->data[2], pFrameYUV->linesize[2]);
#endif	

			SDL_RenderClear(sdlRenderer);
			SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, &sdlRect);
			SDL_RenderPresent(sdlRenderer);
			//SDL End-----------------------
			//Delay 40ms
			SDL_Delay(40);
			//}
		}
		//av_free_packet(packet);
		//av_packet_unref(packet);
		//av_packet_free(&packet);
	}
	//flush decoder
	//FIX: Flush Frames remained in Codec

	//ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);
	ret = avcodec_send_packet(pCodecCtx, NULL);
	while (ret >= 0) {
		ret = avcodec_receive_frame(pCodecCtx, pFrame);
		if (ret < 0)
			break;
		frameCnt++;
		printf("Flush remained frames.\n");
		//if (!got_picture)
			//break;
		sws_scale(img_convert_ctx, (const unsigned char* const*)pFrame->data, pFrame->linesize, 0, pCodecCtx->height,
			pFrameYUV->data, pFrameYUV->linesize);
#if OUTPUT_YUV420P
		int y_size = pCodecCtx->width * pCodecCtx->height;
		fwrite(pFrameYUV->data[0], 1, y_size, fp_yuv);    //Y 
		fwrite(pFrameYUV->data[1], 1, y_size / 4, fp_yuv);  //U
		fwrite(pFrameYUV->data[2], 1, y_size / 4, fp_yuv);  //V
#endif
		//SDL---------------------------
		SDL_UpdateTexture(sdlTexture, &sdlRect, pFrameYUV->data[0], pFrameYUV->linesize[0]);
		SDL_RenderClear(sdlRenderer);
		SDL_RenderCopy(sdlRenderer, sdlTexture, NULL, &sdlRect);
		SDL_RenderPresent(sdlRenderer);
		//SDL End-----------------------
		//Delay 40ms
		SDL_Delay(40);
	}

	sws_freeContext(img_convert_ctx);

#if OUTPUT_YUV420P 
	fclose(fp_yuv);
#endif 

	SDL_Quit();

	av_frame_free(&pFrameYUV);
	av_frame_free(&pFrame);
	av_packet_free(&packet);
	//avcodec_close(pCodecCtx);
	avcodec_free_context(&pCodecCtx);
	avformat_close_input(&pFormatCtx);
	printf("frameCnt : %d", frameCnt);
	return 0;
}
#include <SDL2/SDL.h>

using namespace std;
//引入头文件
extern "C"
{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavdevice/avdevice.h"
//引入时间
#include "libavutil/time.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "SDL2/SDL_vulkan.h"
#include "SDL2/SDL_render.h"
#include "SDL2/SDL_thread.h"
#include "SDL2/SDL_events.h"
#include "SDL2/SDL_quit.h"
#include "SDL2/SDL_timer.h"
}

#define SDL_EVENT_INTERFACE_FRESH (SDL_USEREVENT + 1)
#define SDL_EVENT_QUIT (SDL_USEREVENT + 2)

int thread_exit = 0;

int event_handler(void *data) {
    while (!thread_exit) {
        SDL_Event event;
        event.type = SDL_EVENT_INTERFACE_FRESH;
        SDL_PushEvent(&event);
        SDL_Delay(40);
    }
    SDL_Event event;
    event.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&event);
    return 0;
}

int main() {
    avdevice_register_all();
    AVFormatContext *pContext = avformat_alloc_context();
    AVInputFormat *inFmt = av_find_input_format("avfoundation");
    if (inFmt == nullptr)
        return -1;
    AVDictionary *paramDic;
    av_dict_set(&paramDic, "video_size", "640x480", 0);
    av_dict_set(&paramDic, "framerate", "30", 0);
    av_dict_set(&paramDic, "r", "30", 0);
    av_dict_set(&paramDic, "pixel_format", "uyvy422", 0);
    int ret = avformat_open_input(&pContext, "0:0", inFmt, &paramDic);
    if (ret != 0) {
        printf("Cannot open camera2 \n");
        avformat_free_context(pContext);
        av_dict_free(&paramDic);
        return 0;
    }
    if (avformat_find_stream_info(pContext, nullptr) < 0) {
        printf("Cannot find any stram info.\n");
    }
    int video_index = -1;
    AVStream *videoStream = nullptr;
    for (int i = 0; i != pContext->nb_streams; ++i) {
        if (pContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_index = i;
            videoStream = pContext->streams[i];
            break;
        }
    }
    if (video_index == -1) {
        printf("Cannot find any video stream.\n");
        return -1;
    }
    AVCodec *pCodec = avcodec_find_decoder(videoStream->codecpar->codec_id);
    if (pCodec == nullptr) {
        printf("Cannot find any descoder.\n");
        return -1;
    }
    AVCodecContext *pCodecContext = avcodec_alloc_context3(pCodec);
    avcodec_parameters_to_context(pCodecContext, videoStream->codecpar);
    if (avcodec_open2(pCodecContext, pCodec, nullptr) != 0) {
        printf("Cannot open decoder.\n");
        return -1;
    }
//    分配对应的数据结构
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    AVFrame *frame_yuv = av_frame_alloc();

    int bufSize = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, pCodecContext->width, pCodecContext->height, 1);
    auto *buffer = (uint8_t *) av_malloc((bufSize));
    av_image_fill_arrays(frame_yuv->data, frame_yuv->linesize, (const uint8_t *) buffer, AV_PIX_FMT_YUV420P,
                         pCodecContext->width, pCodecContext->height, 1);
    frame_yuv->format = AV_PIX_FMT_YUV420P;

    SwsContext *swsContext = sws_getContext(pCodecContext->width, pCodecContext->height, pCodecContext->pix_fmt,
                                            pCodecContext->width, pCodecContext->height, AV_PIX_FMT_YUV420P,
                                            SWS_BICUBIC,
                                            nullptr, nullptr, nullptr);
    if (swsContext == nullptr) {
        printf("Cannot get swsContext. \n");
        return -1;
    }

    SDL_Window *sdlWindow = SDL_CreateWindow("Camera 01", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                             pCodecContext->width, pCodecContext->height, SDL_WINDOW_OPENGL);

    SDL_Renderer *sdlRenderer = SDL_CreateRenderer(sdlWindow, -1, 0);
    SDL_Texture *sdlTexture = SDL_CreateTexture(sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,
                                                pCodecContext->width, pCodecContext->height);

    SDL_Rect sdlRect;
    sdlRect.x = sdlRect.y = 0;
    sdlRect.w = pCodecContext->width;
    sdlRect.h = pCodecContext->height;
    FILE *fp = fopen("output.yuv", "wb+");
    SDL_Thread *thread_id = SDL_CreateThread(event_handler, "Camera thread", nullptr);
    SDL_Event event;

    while (true) {
        SDL_WaitEvent(&event);
        if (event.type == SDL_EVENT_INTERFACE_FRESH) {
            if (av_read_frame(pContext, packet) >= 0) {
                if (packet->stream_index == video_index) {
                    int ret = avcodec_send_packet(pCodecContext, packet);
                    if (ret != 0) {
                        printf("Cannot decoder packet.\n");
                        break;
                    }
                    if (avcodec_receive_frame(pCodecContext, frame) >= 0) {
                        sws_scale(swsContext, (const uint8_t *const *) frame->data, frame->linesize, 0,
                                  pCodecContext->height, frame_yuv->data, frame_yuv->linesize);
                    }
                    if (frame_yuv->format == AV_PIX_FMT_YUV420P) {
                        SDL_UpdateYUVTexture(sdlTexture, &sdlRect, frame_yuv->data[0], frame_yuv->linesize[0],
                                             frame_yuv->data[1], frame_yuv->linesize[1], frame_yuv->data[2],
                                             frame_yuv->linesize[2]);
                        if (fp != nullptr) {
                            fwrite(frame_yuv->data[0], 1, pCodecContext->width * pCodecContext->height, fp);
                            fwrite(frame_yuv->data[1], 1, pCodecContext->width * pCodecContext->height / 4, fp);
                            fwrite(frame_yuv->data[2], 1, pCodecContext->width * pCodecContext->height / 4, fp);
                        }
                    }
                    SDL_RenderClear(sdlRenderer);
                    SDL_RenderCopy(sdlRenderer, sdlTexture, nullptr, &sdlRect);
                    SDL_RenderPresent(sdlRenderer);
                }
            }
        } else if (event.type == SDL_EVENT_QUIT) {
            break;
        } else if (event.type == SDL_KEYDOWN) {
            const uint8_t *statue = SDL_GetKeyboardState(nullptr);
            if (statue[SDL_SCANCODE_Q]) {
                thread_exit = 1;
            }
            break;
        }
    }

    fclose(fp);
    SDL_Quit();
    avcodec_close(pCodecContext);
    avformat_free_context(pContext);
    return 0;
}
cmake_minimum_required(VERSION 3.16)
project(hc_demo)
set(CMAKE_CXX_STANDARD 14)

set(SRC_DIR ./)
set(SDL_DIR /usr/local/Cellar/sdl2/2.0.12_1/)
set(FFMPEG_DIR /usr/local/Cellar/ffmpeg/4.2.3_1/)

# ffmpeg 库
include_directories(${FFMPEG_DIR}/include/)
link_directories(${FFMPEG_DIR}/lib/)

# SDL 链接库
include_directories(${SDL_DIR}/include/)
link_directories(${SDL_DIR}/lib/)

add_executable(hc_demo src/main.cpp test/rtmp/test_rtmp_pusher.cpp test/rtmp/test_rtmp_pusher.h)

target_link_libraries(
        hc_demo
        SDL2
        SDL2_test
        SDL2main
        avcodec.58
        avdevice.58
        avfilter.7
        avformat.58
        avutil.56
        postproc.55
        swresample.3
        swscale.5
)
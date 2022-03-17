# CMakeList.txt: Algorithm 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#
cmake_minimum_required (VERSION 3.8)

project ("simplest_ffmpeg_player")

include_directories(include)

link_directories("${PROJECT_SOURCE_DIR}/lib")


add_executable(ffmpeg_test "ffmpeg_test/ffmpeg_test.cpp" )

target_link_libraries(
	ffmpeg_test
	avfilter
    avformat
    avcodec
    avutil
    swresample
    swscale
)

add_executable(simplest_ffmpeg_player "simplest_ffmpeg_player/simplest_ffmpeg_player.cpp")

target_link_libraries(
	simplest_ffmpeg_player
	avfilter
    avformat
    avcodec
    avutil
    swresample
    swscale
    SDL2
    SDL2main
)

add_executable(simplest_ffmpeg_decoder_pure "simplest_ffmpeg_decoder_pure/simplest_ffmpeg_decoder_pure.cpp")

target_link_libraries(
	simplest_ffmpeg_decoder_pure
	avfilter
    avformat
    avcodec
    avutil
    swresample
    swscale
)

add_executable(simplest_ffmpeg_audio_player "simplest_ffmpeg_audio_player/simplest_ffmpeg_audio_player.cpp")

target_link_libraries(
	simplest_ffmpeg_audio_player
	avfilter
    avformat
    avcodec
    avutil
    swresample
    swscale
    SDL2
    SDL2main
)

file(GLOB SPEED_SRCS simplest_ffmpeg_audio_player_speed/*.cpp)

add_executable(simplest_ffmpeg_audio_player_speed ${SPEED_SRCS})


target_link_libraries(
	simplest_ffmpeg_audio_player_speed
	avfilter
    avformat
    avcodec
    avutil
    swresample
    swscale
    SDL2
    SDL2main
)



file(COPY "${PROJECT_SOURCE_DIR}/dll/" DESTINATION  "${PROJECT_BINARY_DIR}")




# 将源代码添加到此项目的可执行文件。
# add_executable (simplest_ffmpeg_player "simplest_ffmpeg_player/simplest_ffmpeg_player.cpp")

# TODO: 如有需要，请添加测试并安装目标。

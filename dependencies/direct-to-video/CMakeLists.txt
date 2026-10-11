cmake_minimum_required(VERSION 3.16)

project(direct-to-video LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

find_package(FFmpeg REQUIRED)

add_library(direct-to-video STATIC
    src/frame.cpp
    src/frame_queue.cpp
    src/encoder.cpp

    include/frame.h
    include/ffmpeg.h
    include/frame_queue.h
    include/encoder.h
    include/dtv.h
)

target_include_directories(direct-to-video PUBLIC
    ${AVCODEC_INCLUDE_DIR}
    ${AVFORMAT_INCLUDE_DIR}
    ${AVUTIL_INCLUDE_DIR}
    ${AVDEVICE_INCLUDE_DIR}
)

target_link_libraries(direct-to-video PUBLIC
    ${AVCODEC_LIBRARY}
    ${AVFORMAT_LIBRARY}
    ${AVUTIL_LIBRARY}
    ${AVDEVICE_LIBRARY}
    ${SWSCALE_LIBRARY}
    ${SWRESAMPLE_LIBRARY}
)

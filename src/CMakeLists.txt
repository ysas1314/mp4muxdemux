cmake_minimum_required(VERSION 3.8)
project(mp4demux)

set(CMAKE_CXX_STANDARD 11)

set(faad2_sdk  /root/work/code/github/mp4v2_demo/mp4muxdmux/mp4muxdemux/third_lib/faad2-2.7-x86-64-gnu/)
set(mp4v2_sdk  /root/work/code/github/mp4v2_demo/mp4muxdmux/mp4muxdemux/third_lib/mp4v2-x86-64-gun/)
include_directories(${faad2_sdk}include/ ${mp4v2_sdk}/include/)
link_directories(${faad2_sdk}lib/ ${mp4v2_sdk}lib/)
set(EXECUTABLE_OUTPUT_PATH  /work/code/github/mp4v2_demo/mp4muxdmux/mp4muxdemux/bin)

set(SOURCE_FILES main.cpp CMp4Decoder.cpp CMp4Decoder.h AacDecode.cpp AacDecode.h MP4Encoder.cpp MP4Encoder.h)

add_executable(mp4demux ${SOURCE_FILES})

target_link_libraries(mp4demux libmp4v2.so  libfaad.so)
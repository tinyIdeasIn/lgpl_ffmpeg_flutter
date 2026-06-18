cmake_minimum_required(VERSION 3.22.1)

project(lgpl_ffmpeg_flutter)

add_library(lgpl_ffmpeg_flutter_backend SHARED lgpl_ffmpeg_flutter_backend.cpp)

target_link_options(
  lgpl_ffmpeg_flutter_backend
  PRIVATE
  "-Wl,-z,max-page-size=16384"
)

target_include_directories(
  lgpl_ffmpeg_flutter_backend
  PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

find_library(log-lib log)

set(FFMPEG_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/${ANDROID_ABI})

target_link_libraries(
  lgpl_ffmpeg_flutter_backend
  ${FFMPEG_LIB_DIR}/libavformat.so
  ${FFMPEG_LIB_DIR}/libavcodec.so
  ${FFMPEG_LIB_DIR}/libswscale.so
  ${FFMPEG_LIB_DIR}/libswresample.so
  ${FFMPEG_LIB_DIR}/libavutil.so
  ${log-lib}
)

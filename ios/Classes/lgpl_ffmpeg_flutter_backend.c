#include "lgpl_ffmpeg_flutter_backend.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/display.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

const char* lgpl_ffmpeg_flutter_backend_status(void) {
  return "LGPL FFmpeg backend linked.";
}

static char* duplicate_string(const char* value) {
  size_t length = strlen(value);
  char* result = (char*)malloc(length + 1);
  memcpy(result, value, length + 1);
  return result;
}

static char* error_json(const char* code, const char* message) {
  char buffer[512];
  snprintf(buffer, sizeof(buffer), "{\"errorCode\":\"%s\",\"message\":\"%s\"}",
           code, message);
  return duplicate_string(buffer);
}

static long long duration_ms(const AVFormatContext* format_context) {
  if (format_context->duration == AV_NOPTS_VALUE) {
    return 0;
  }
  return format_context->duration * 1000 / AV_TIME_BASE;
}

static int rotation_for_stream(const AVStream* stream) {
  const AVPacketSideData* display_matrix =
      av_packet_side_data_get(stream->codecpar->coded_side_data,
                              stream->codecpar->nb_coded_side_data,
                              AV_PKT_DATA_DISPLAYMATRIX);
  if (display_matrix != NULL) {
    double angle =
        av_display_rotation_get((const int32_t*)display_matrix->data);
    if (!isnan(angle)) {
      int rotation = (int)-angle % 360;
      if (rotation < 0) {
        rotation += 360;
      }
      return rotation;
    }
  }
  AVDictionaryEntry* rotate =
      av_dict_get(stream->metadata, "rotate", NULL, 0);
  return rotate == NULL ? 0 : atoi(rotate->value);
}

static const char* mime_type_for_input(const char* path,
                                       const AVInputFormat* input_format) {
  if (input_format != NULL && input_format->mime_type != NULL) {
    return input_format->mime_type;
  }
  const char* dot = strrchr(path, '.');
  if (dot == NULL) {
    return "";
  }
  if (strcasecmp(dot, ".mp4") == 0 || strcasecmp(dot, ".m4v") == 0) {
    return "video/mp4";
  }
  if (strcasecmp(dot, ".mov") == 0) {
    return "video/quicktime";
  }
  if (strcasecmp(dot, ".webm") == 0) {
    return "video/webm";
  }
  return "";
}

static int open_video(const char* path,
                      AVFormatContext** format_context,
                      int* video_stream_index) {
  *format_context = NULL;
  *video_stream_index = -1;
  if (avformat_open_input(format_context, path, NULL, NULL) < 0) {
    return 0;
  }
  if (avformat_find_stream_info(*format_context, NULL) < 0) {
    avformat_close_input(format_context);
    return 0;
  }
  int index =
      av_find_best_stream(*format_context, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
  if (index >= 0) {
    *video_stream_index = index;
  }
  return 1;
}

char* lgpl_ffmpeg_flutter_read_info(const char* video_path) {
  AVFormatContext* format_context = NULL;
  int video_stream_index = -1;
  if (!open_video(video_path, &format_context, &video_stream_index)) {
    return error_json("openFailed", "Could not open video.");
  }
  if (video_stream_index < 0) {
    avformat_close_input(&format_context);
    return error_json("noVideoStream", "No video stream found.");
  }

  AVStream* stream = format_context->streams[video_stream_index];
  AVCodecParameters* codecpar = stream->codecpar;
  char buffer[1024];
  const char* mime_type = mime_type_for_input(video_path, format_context->iformat);
  if (format_context->bit_rate > 0) {
    snprintf(buffer, sizeof(buffer),
             "{\"durationMs\":%lld,\"width\":%d,\"height\":%d,"
             "\"rotation\":%d,\"bitrate\":%lld,\"mimeType\":\"%s\"}",
             duration_ms(format_context), codecpar->width, codecpar->height,
             rotation_for_stream(stream), (long long)format_context->bit_rate,
             mime_type);
  } else {
    snprintf(buffer, sizeof(buffer),
             "{\"durationMs\":%lld,\"width\":%d,\"height\":%d,"
             "\"rotation\":%d,\"bitrate\":null,\"mimeType\":\"%s\"}",
             duration_ms(format_context), codecpar->width, codecpar->height,
             rotation_for_stream(stream), mime_type);
  }
  avformat_close_input(&format_context);
  return duplicate_string(buffer);
}

static int decode_frame_at(AVFormatContext* format_context,
                           AVStream* stream,
                           AVCodecContext* codec_context,
                           long long target_ms,
                           AVFrame* decoded_frame) {
  int64_t timestamp =
      av_rescale_q(target_ms, (AVRational){1, 1000}, stream->time_base);
  av_seek_frame(format_context, stream->index, timestamp, AVSEEK_FLAG_BACKWARD);
  avcodec_flush_buffers(codec_context);

  AVPacket* packet = av_packet_alloc();
  int decoded = 0;
  while (av_read_frame(format_context, packet) >= 0) {
    if (packet->stream_index == stream->index &&
        avcodec_send_packet(codec_context, packet) == 0) {
      if (avcodec_receive_frame(codec_context, decoded_frame) == 0) {
        decoded = 1;
        av_packet_unref(packet);
        break;
      }
    }
    av_packet_unref(packet);
  }
  av_packet_free(&packet);
  return decoded;
}

static int write_png_file(const char* output_path, const AVFrame* rgb_frame) {
  const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_PNG);
  if (codec == NULL) {
    return 0;
  }

  AVCodecContext* codec_context = avcodec_alloc_context3(codec);
  if (codec_context == NULL) {
    return 0;
  }
  codec_context->width = rgb_frame->width;
  codec_context->height = rgb_frame->height;
  codec_context->pix_fmt = AV_PIX_FMT_RGB24;
  codec_context->time_base = (AVRational){1, 1};
  if (avcodec_open2(codec_context, codec, NULL) < 0) {
    avcodec_free_context(&codec_context);
    return 0;
  }

  AVPacket* packet = av_packet_alloc();
  if (packet == NULL) {
    avcodec_free_context(&codec_context);
    return 0;
  }

  int success = 0;
  if (avcodec_send_frame(codec_context, rgb_frame) == 0 &&
      avcodec_receive_packet(codec_context, packet) == 0) {
    FILE* file = fopen(output_path, "wb");
    if (file != NULL) {
      success = fwrite(packet->data, 1, packet->size, file) == (size_t)packet->size;
      fclose(file);
    }
  }

  av_packet_free(&packet);
  avcodec_free_context(&codec_context);
  return success;
}

char* lgpl_ffmpeg_flutter_generate_cover(const char* video_path,
                                         const long long* preferred_times_ms,
                                         int preferred_times_count,
                                         int max_long_edge,
                                         int quality,
                                         const char* cache_dir) {
  (void)quality;
  AVFormatContext* format_context = NULL;
  int video_stream_index = -1;
  if (!open_video(video_path, &format_context, &video_stream_index)) {
    return error_json("openFailed", "Could not open video.");
  }
  if (video_stream_index < 0) {
    avformat_close_input(&format_context);
    return error_json("noVideoStream", "No video stream found.");
  }

  AVStream* stream = format_context->streams[video_stream_index];
  const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
  if (codec == NULL) {
    avformat_close_input(&format_context);
    return error_json("decodeFailed", "No decoder found for video stream.");
  }

  AVCodecContext* codec_context = avcodec_alloc_context3(codec);
  avcodec_parameters_to_context(codec_context, stream->codecpar);
  if (avcodec_open2(codec_context, codec, NULL) < 0) {
    avcodec_free_context(&codec_context);
    avformat_close_input(&format_context);
    return error_json("decodeFailed", "Could not open video decoder.");
  }

  long long default_times[4] = {300, 1000, 0, 0};
  int default_count = 2;
  long long total_duration = duration_ms(format_context);
  if (total_duration > 0) {
    default_times[2] = total_duration / 10;
    default_times[3] = total_duration * 3 / 10;
    default_count = 4;
  }

  const long long* candidates =
      preferred_times_count > 0 ? preferred_times_ms : default_times;
  int candidate_count =
      preferred_times_count > 0 ? preferred_times_count : default_count;

  AVFrame* decoded_frame = av_frame_alloc();
  int decoded = 0;
  for (int i = 0; i < candidate_count; i++) {
    long long candidate = candidates[i] < 0 ? 0 : candidates[i];
    decoded =
        decode_frame_at(format_context, stream, codec_context, candidate, decoded_frame);
    if (decoded) {
      break;
    }
  }

  if (!decoded) {
    av_frame_free(&decoded_frame);
    avcodec_free_context(&codec_context);
    avformat_close_input(&format_context);
    return duplicate_string("{\"coverPath\":null}");
  }

  int source_width = decoded_frame->width;
  int source_height = decoded_frame->height;
  int long_edge = source_width > source_height ? source_width : source_height;
  double scale = long_edge > max_long_edge ? (double)max_long_edge / long_edge : 1.0;
  int out_width = source_width * scale;
  int out_height = source_height * scale;
  if (out_width < 1) out_width = 1;
  if (out_height < 1) out_height = 1;

  AVFrame* rgb_frame = av_frame_alloc();
  rgb_frame->format = AV_PIX_FMT_RGB24;
  rgb_frame->width = out_width;
  rgb_frame->height = out_height;
  av_frame_get_buffer(rgb_frame, 32);

  struct SwsContext* sws_context =
      sws_getContext(source_width, source_height, codec_context->pix_fmt, out_width,
                     out_height, AV_PIX_FMT_RGB24, SWS_BILINEAR, NULL, NULL, NULL);
  if (sws_context == NULL) {
    av_frame_free(&rgb_frame);
    av_frame_free(&decoded_frame);
    avcodec_free_context(&codec_context);
    avformat_close_input(&format_context);
    return error_json("outputFailed", "Could not create scaler.");
  }

  sws_scale(sws_context, (const uint8_t* const*)decoded_frame->data,
            decoded_frame->linesize, 0, source_height, rgb_frame->data,
            rgb_frame->linesize);

  char output_path[1024];
  snprintf(output_path, sizeof(output_path), "%s/lgpl_ffmpeg_cover_%ld_%d.png",
           cache_dir, time(NULL), rand());
  if (!write_png_file(output_path, rgb_frame)) {
    sws_freeContext(sws_context);
    av_frame_free(&rgb_frame);
    av_frame_free(&decoded_frame);
    avcodec_free_context(&codec_context);
    avformat_close_input(&format_context);
    return error_json("outputFailed", "Could not create cover file.");
  }

  sws_freeContext(sws_context);
  av_frame_free(&rgb_frame);
  av_frame_free(&decoded_frame);
  avcodec_free_context(&codec_context);
  avformat_close_input(&format_context);

  char result[1200];
  snprintf(result, sizeof(result), "{\"coverPath\":\"%s\"}", output_path);
  return duplicate_string(result);
}

void lgpl_ffmpeg_flutter_free_string(char* value) {
  free(value);
}

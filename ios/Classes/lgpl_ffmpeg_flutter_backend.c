#include "lgpl_ffmpeg_flutter_backend.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/display.h>
#include <libavutil/ffversion.h>
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

static char* json_escape(const char* value) {
  size_t extra = 0;
  for (const char* cursor = value; *cursor != '\0'; cursor++) {
    if (*cursor == '"' || *cursor == '\\' || *cursor == '\n' ||
        *cursor == '\r' || *cursor == '\t') {
      extra++;
    }
  }

  size_t length = strlen(value);
  char* escaped = (char*)malloc(length + extra + 1);
  if (escaped == NULL) {
    return NULL;
  }

  char* out = escaped;
  for (const char* cursor = value; *cursor != '\0'; cursor++) {
    switch (*cursor) {
      case '"':
        *out++ = '\\';
        *out++ = '"';
        break;
      case '\\':
        *out++ = '\\';
        *out++ = '\\';
        break;
      case '\n':
        *out++ = '\\';
        *out++ = 'n';
        break;
      case '\r':
        *out++ = '\\';
        *out++ = 'r';
        break;
      case '\t':
        *out++ = '\\';
        *out++ = 't';
        break;
      default:
        *out++ = *cursor;
        break;
    }
  }
  *out = '\0';
  return escaped;
}

static void version_string(unsigned version, char* buffer, size_t buffer_size) {
  snprintf(buffer, buffer_size, "%d.%d.%d", AV_VERSION_MAJOR(version),
           AV_VERSION_MINOR(version), AV_VERSION_MICRO(version));
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

static int normalize_right_angle(int rotation) {
  rotation %= 360;
  if (rotation < 0) {
    rotation += 360;
  }
  return ((rotation + 45) / 90 * 90) % 360;
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

char* lgpl_ffmpeg_flutter_backend_info(void) {
  char avformat_version_text[32];
  char avcodec_version_text[32];
  char avutil_version_text[32];
  version_string(avformat_version(), avformat_version_text,
                 sizeof(avformat_version_text));
  version_string(avcodec_version(), avcodec_version_text,
                 sizeof(avcodec_version_text));
  version_string(avutil_version(), avutil_version_text,
                 sizeof(avutil_version_text));

  const char* configuration = avformat_configuration();
  const char* license = avformat_license();
  char* escaped_configuration = json_escape(configuration);
  char* escaped_license = json_escape(license);
  if (escaped_configuration == NULL || escaped_license == NULL) {
    free(escaped_configuration);
    free(escaped_license);
    return NULL;
  }
  size_t capacity = strlen(escaped_configuration) + strlen(escaped_license) + 512;
  char* result = (char*)malloc(capacity);
  if (result == NULL) {
    free(escaped_configuration);
    free(escaped_license);
    return NULL;
  }
  snprintf(result, capacity,
           "{\"ffmpegVersion\":\"%s\",\"avformatVersion\":\"%s\","
           "\"avcodecVersion\":\"%s\",\"avutilVersion\":\"%s\","
           "\"configuration\":\"%s\",\"license\":\"%s\"}",
           FFMPEG_VERSION, avformat_version_text, avcodec_version_text,
           avutil_version_text, escaped_configuration, escaped_license);
  free(escaped_configuration);
  free(escaped_license);
  return result;
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

static AVFrame* allocate_rgb_frame(int width, int height) {
  AVFrame* frame = av_frame_alloc();
  if (frame == NULL) {
    return NULL;
  }
  frame->format = AV_PIX_FMT_RGB24;
  frame->width = width;
  frame->height = height;
  if (av_frame_get_buffer(frame, 32) < 0) {
    av_frame_free(&frame);
    return NULL;
  }
  return frame;
}

static void copy_rgb_pixel(const AVFrame* source,
                           AVFrame* destination,
                           int source_x,
                           int source_y,
                           int destination_x,
                           int destination_y) {
  const uint8_t* source_pixel =
      source->data[0] + source_y * source->linesize[0] + source_x * 3;
  uint8_t* destination_pixel = destination->data[0] +
                               destination_y * destination->linesize[0] +
                               destination_x * 3;
  destination_pixel[0] = source_pixel[0];
  destination_pixel[1] = source_pixel[1];
  destination_pixel[2] = source_pixel[2];
}

static AVFrame* rotate_rgb_frame(const AVFrame* source, int rotation) {
  int normalized_rotation = normalize_right_angle(rotation);
  if (normalized_rotation == 0) {
    return NULL;
  }

  int swaps_axes = normalized_rotation == 90 || normalized_rotation == 270;
  AVFrame* rotated = allocate_rgb_frame(swaps_axes ? source->height : source->width,
                                        swaps_axes ? source->width : source->height);
  if (rotated == NULL) {
    return NULL;
  }

  for (int y = 0; y < source->height; y++) {
    for (int x = 0; x < source->width; x++) {
      int destination_x = x;
      int destination_y = y;
      if (normalized_rotation == 90) {
        destination_x = source->height - 1 - y;
        destination_y = x;
      } else if (normalized_rotation == 180) {
        destination_x = source->width - 1 - x;
        destination_y = source->height - 1 - y;
      } else if (normalized_rotation == 270) {
        destination_x = y;
        destination_y = source->width - 1 - x;
      }
      copy_rgb_pixel(source, rotated, x, y, destination_x, destination_y);
    }
  }

  return rotated;
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
  int rotation = normalize_right_angle(rotation_for_stream(stream));
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

  AVFrame* rgb_frame = allocate_rgb_frame(out_width, out_height);
  if (rgb_frame == NULL) {
    av_frame_free(&decoded_frame);
    avcodec_free_context(&codec_context);
    avformat_close_input(&format_context);
    return error_json("outputFailed", "Could not allocate cover frame.");
  }

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

  AVFrame* cover_frame = rgb_frame;
  AVFrame* rotated_frame = rotate_rgb_frame(rgb_frame, rotation);
  if (rotation != 0) {
    if (rotated_frame == NULL) {
      sws_freeContext(sws_context);
      av_frame_free(&rgb_frame);
      av_frame_free(&decoded_frame);
      avcodec_free_context(&codec_context);
      avformat_close_input(&format_context);
      return error_json("outputFailed", "Could not rotate cover frame.");
    }
    cover_frame = rotated_frame;
  }

  char output_path[1024];
  snprintf(output_path, sizeof(output_path), "%s/lgpl_ffmpeg_cover_%ld_%d.png",
           cache_dir, time(NULL), rand());
  if (!write_png_file(output_path, cover_frame)) {
    sws_freeContext(sws_context);
    if (rotated_frame != NULL) {
      av_frame_free(&rotated_frame);
    }
    av_frame_free(&rgb_frame);
    av_frame_free(&decoded_frame);
    avcodec_free_context(&codec_context);
    avformat_close_input(&format_context);
    return error_json("outputFailed", "Could not create cover file.");
  }

  sws_freeContext(sws_context);
  if (rotated_frame != NULL) {
    av_frame_free(&rotated_frame);
  }
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

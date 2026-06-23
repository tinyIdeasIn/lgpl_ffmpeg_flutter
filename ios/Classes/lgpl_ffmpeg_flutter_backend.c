#include "lgpl_ffmpeg_flutter_backend.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/display.h>
#include <libavutil/ffversion.h>
#include <libavutil/imgutils.h>
#include <libavutil/mastering_display_metadata.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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

static long long file_size_bytes(const char* path) {
  struct stat stat_buffer;
  if (stat(path, &stat_buffer) != 0 || stat_buffer.st_size < 0) {
    return -1;
  }
  return (long long)stat_buffer.st_size;
}

static const char* codec_name_for_stream(const AVStream* stream) {
  if (stream == NULL || stream->codecpar == NULL) {
    return "";
  }
  const char* name = avcodec_get_name(stream->codecpar->codec_id);
  return name == NULL ? "" : name;
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
  int audio_stream_index =
      av_find_best_stream(format_context, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
  AVStream* audio_stream =
      audio_stream_index >= 0 ? format_context->streams[audio_stream_index] : NULL;
  char buffer[1536];
  const char* mime_type = mime_type_for_input(video_path, format_context->iformat);
  const char* format_name =
      format_context->iformat != NULL && format_context->iformat->name != NULL
          ? format_context->iformat->name
          : "";
  const char* video_codec = codec_name_for_stream(stream);
  const char* audio_codec = codec_name_for_stream(audio_stream);
  long long size_bytes = file_size_bytes(video_path);
  const char* audio_codec_prefix = strlen(audio_codec) > 0 ? "\"" : "";
  const char* audio_codec_suffix = strlen(audio_codec) > 0 ? "\"" : "";
  char file_size_json[32];
  if (size_bytes >= 0) {
    snprintf(file_size_json, sizeof(file_size_json), "%lld", size_bytes);
  } else {
    snprintf(file_size_json, sizeof(file_size_json), "null");
  }
  if (format_context->bit_rate > 0) {
    snprintf(buffer, sizeof(buffer),
             "{\"durationMs\":%lld,\"width\":%d,\"height\":%d,"
             "\"rotation\":%d,\"bitrate\":%lld,\"mimeType\":\"%s\","
             "\"formatName\":\"%s\",\"videoCodec\":\"%s\","
             "\"audioCodec\":%s%s%s,\"fileSizeBytes\":%s}",
             duration_ms(format_context), codecpar->width, codecpar->height,
             rotation_for_stream(stream), (long long)format_context->bit_rate,
             mime_type, format_name, video_codec, audio_codec_prefix,
             strlen(audio_codec) > 0 ? audio_codec : "null", audio_codec_suffix,
             file_size_json);
  } else {
    snprintf(buffer, sizeof(buffer),
             "{\"durationMs\":%lld,\"width\":%d,\"height\":%d,"
             "\"rotation\":%d,\"bitrate\":null,\"mimeType\":\"%s\","
             "\"formatName\":\"%s\",\"videoCodec\":\"%s\","
             "\"audioCodec\":%s%s%s,\"fileSizeBytes\":%s}",
             duration_ms(format_context), codecpar->width, codecpar->height,
             rotation_for_stream(stream), mime_type, format_name, video_codec,
             audio_codec_prefix, strlen(audio_codec) > 0 ? audio_codec : "null",
             audio_codec_suffix, file_size_json);
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
           "\"configuration\":\"%s\",\"license\":\"%s\","
           "\"supportedInputFormats\":[\"mov\",\"matroska\","
           "\"webm_dash_manifest\"],"
           "\"supportedVideoDecoders\":[\"h264\",\"hevc\",\"mpeg4\","
           "\"vp8\",\"vp9\"],"
           "\"supportedAudioDecoders\":[\"aac\"],"
           "\"outputImageFormat\":\"png\"}",
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

static long long frame_time_ms(const AVFrame* frame, const AVStream* stream) {
  if (frame == NULL || stream == NULL ||
      frame->best_effort_timestamp == AV_NOPTS_VALUE) {
    return -1;
  }
  return av_rescale_q(frame->best_effort_timestamp, stream->time_base,
                      (AVRational){1, 1000});
}

static double clamp_double(double value, double min_value, double max_value) {
  if (value < min_value) {
    return min_value;
  }
  if (value > max_value) {
    return max_value;
  }
  return value;
}

static int clamp_int(int value, int min_value, int max_value) {
  if (value < min_value) {
    return min_value;
  }
  if (value > max_value) {
    return max_value;
  }
  return value;
}

static enum AVColorTransferCharacteristic transfer_for_frame(
    const AVFrame* frame,
    const AVCodecContext* codec_context) {
  enum AVColorTransferCharacteristic transfer = frame->color_trc;
  if (transfer == AVCOL_TRC_UNSPECIFIED && codec_context != NULL) {
    transfer = codec_context->color_trc;
  }
  return transfer;
}

static enum AVColorPrimaries primaries_for_frame(
    const AVFrame* frame,
    const AVCodecContext* codec_context) {
  enum AVColorPrimaries primaries = frame->color_primaries;
  if (primaries == AVCOL_PRI_UNSPECIFIED && codec_context != NULL) {
    primaries = codec_context->color_primaries;
  }
  return primaries;
}

static enum AVColorSpace color_space_for_frame(
    const AVFrame* frame,
    const AVCodecContext* codec_context) {
  enum AVColorSpace color_space = frame->colorspace;
  if (color_space == AVCOL_SPC_UNSPECIFIED && codec_context != NULL) {
    color_space = codec_context->colorspace;
  }
  return color_space;
}

static int bits_per_component_for_frame(const AVFrame* frame,
                                        const AVCodecContext* codec_context) {
  enum AVPixelFormat format = (enum AVPixelFormat)frame->format;
  if (format == AV_PIX_FMT_NONE && codec_context != NULL) {
    format = codec_context->pix_fmt;
  }
  const AVPixFmtDescriptor* descriptor = av_pix_fmt_desc_get(format);
  return descriptor != NULL ? descriptor->comp[0].depth : 0;
}

static int has_hdr_side_data(const AVFrame* frame) {
  return av_frame_get_side_data(frame, AV_FRAME_DATA_MASTERING_DISPLAY_METADATA) !=
             NULL ||
         av_frame_get_side_data(frame, AV_FRAME_DATA_CONTENT_LIGHT_LEVEL) != NULL;
}

static int needs_hdr_tone_mapping(const AVFrame* frame,
                                  const AVCodecContext* codec_context) {
  enum AVColorTransferCharacteristic transfer =
      transfer_for_frame(frame, codec_context);
  if (transfer == AVCOL_TRC_SMPTE2084 || transfer == AVCOL_TRC_ARIB_STD_B67) {
    return 1;
  }
  if (transfer != AVCOL_TRC_UNSPECIFIED) {
    return 0;
  }

  if (has_hdr_side_data(frame)) {
    return 1;
  }
  enum AVColorSpace color_space = color_space_for_frame(frame, codec_context);
  return primaries_for_frame(frame, codec_context) == AVCOL_PRI_BT2020 &&
         (color_space == AVCOL_SPC_BT2020_NCL ||
          color_space == AVCOL_SPC_BT2020_CL) &&
         bits_per_component_for_frame(frame, codec_context) >= 10;
}

static double pq_to_nits(double encoded) {
  const double m1 = 2610.0 / 16384.0;
  const double m2 = 2523.0 / 32.0;
  const double c1 = 3424.0 / 4096.0;
  const double c2 = 2413.0 / 128.0;
  const double c3 = 2392.0 / 128.0;
  double power = pow(clamp_double(encoded, 0.0, 1.0), 1.0 / m2);
  double numerator = fmax(power - c1, 0.0);
  double denominator = fmax(c2 - c3 * power, 1e-9);
  return 10000.0 * pow(numerator / denominator, 1.0 / m1);
}

static double hlg_to_nits(double encoded) {
  const double a = 0.17883277;
  const double b = 0.28466892;
  const double c = 0.55991073;
  encoded = clamp_double(encoded, 0.0, 1.0);
  double linear =
      encoded <= 0.5 ? encoded * encoded / 3.0 : (exp((encoded - c) / a) + b) / 12.0;
  return linear * 1000.0;
}

static double tone_map_nits_to_sdr_with_knee(double nits, double knee_nits) {
  nits = fmax(0.0, nits);
  return clamp_double(nits / (nits + knee_nits), 0.0, 1.0);
}

static double srgb_encode(double linear) {
  linear = clamp_double(linear, 0.0, 1.0);
  if (linear <= 0.0031308) {
    return linear * 12.92;
  }
  return 1.055 * pow(linear, 1.0 / 2.4) - 0.055;
}

static void convert_bt2020_to_bt709(double* r, double* g, double* b) {
  double source_r = *r;
  double source_g = *g;
  double source_b = *b;
  *r = 1.6605 * source_r - 0.5876 * source_g - 0.0728 * source_b;
  *g = -0.1246 * source_r + 1.1329 * source_g - 0.0083 * source_b;
  *b = -0.0182 * source_r - 0.1006 * source_g + 1.1187 * source_b;
}

static int sws_color_space_for_frame(const AVFrame* frame,
                                     const AVCodecContext* codec_context) {
  enum AVColorSpace color_space = color_space_for_frame(frame, codec_context);

  switch (color_space) {
    case AVCOL_SPC_BT709:
      return SWS_CS_ITU709;
    case AVCOL_SPC_SMPTE170M:
    case AVCOL_SPC_BT470BG:
      return SWS_CS_SMPTE170M;
    case AVCOL_SPC_SMPTE240M:
      return SWS_CS_SMPTE240M;
    case AVCOL_SPC_BT2020_NCL:
    case AVCOL_SPC_BT2020_CL:
      return SWS_CS_BT2020;
    default:
      return SWS_CS_DEFAULT;
  }
}

static int configure_sws_colorspace(struct SwsContext* sws_context,
                                    const AVFrame* frame,
                                    const AVCodecContext* codec_context) {
  enum AVColorRange color_range = frame->color_range;
  if (color_range == AVCOL_RANGE_UNSPECIFIED && codec_context != NULL) {
    color_range = codec_context->color_range;
  }

  int source_range = color_range == AVCOL_RANGE_JPEG ? 1 : 0;
  const int* coefficients =
      sws_getCoefficients(sws_color_space_for_frame(frame, codec_context));
  return sws_setColorspaceDetails(sws_context, coefficients, source_range,
                                  sws_getCoefficients(SWS_CS_DEFAULT), 1, 0,
                                  1 << 16, 1 << 16) >= 0;
}

static double max_mastering_luminance_nits(const AVFrame* frame) {
  const AVFrameSideData* side_data =
      av_frame_get_side_data(frame, AV_FRAME_DATA_MASTERING_DISPLAY_METADATA);
  if (side_data == NULL || side_data->data == NULL) {
    return 0.0;
  }
  const AVMasteringDisplayMetadata* metadata =
      (const AVMasteringDisplayMetadata*)side_data->data;
  if (!metadata->has_luminance) {
    return 0.0;
  }
  return av_q2d(metadata->max_luminance);
}

static double max_content_light_level_nits(const AVFrame* frame) {
  const AVFrameSideData* side_data =
      av_frame_get_side_data(frame, AV_FRAME_DATA_CONTENT_LIGHT_LEVEL);
  if (side_data == NULL || side_data->data == NULL) {
    return 0.0;
  }
  const AVContentLightMetadata* metadata =
      (const AVContentLightMetadata*)side_data->data;
  return (double)metadata->MaxCLL;
}

static double hdr_metadata_peak_nits(const AVFrame* frame) {
  double mastering_max = max_mastering_luminance_nits(frame);
  double max_cll = max_content_light_level_nits(frame);
  double peak = mastering_max > max_cll ? mastering_max : max_cll;
  return peak > 0.0 ? clamp_double(peak, 400.0, 10000.0) : 0.0;
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

static AVFrame* allocate_rgb48_frame(int width, int height) {
  AVFrame* frame = av_frame_alloc();
  if (frame == NULL) {
    return NULL;
  }
  frame->format = AV_PIX_FMT_RGB48LE;
  frame->width = width;
  frame->height = height;
  if (av_frame_get_buffer(frame, 32) < 0) {
    av_frame_free(&frame);
    return NULL;
  }
  return frame;
}

static double* build_hdr_to_nits_table(
    enum AVColorTransferCharacteristic transfer) {
  double* table = (double*)malloc(sizeof(double) * 65536);
  if (table == NULL) {
    return NULL;
  }
  for (int i = 0; i <= 65535; i++) {
    double encoded = (double)i / 65535.0;
    table[i] = transfer == AVCOL_TRC_ARIB_STD_B67 ? hlg_to_nits(encoded)
                                                   : pq_to_nits(encoded);
  }
  return table;
}

static uint8_t* build_srgb_encode_table(void) {
  uint8_t* table = (uint8_t*)malloc(sizeof(uint8_t) * 4097);
  if (table == NULL) {
    return NULL;
  }
  for (int i = 0; i <= 4096; i++) {
    table[i] = (uint8_t)lrint(srgb_encode((double)i / 4096.0) * 255.0);
  }
  return table;
}

static double estimate_peak_luminance_nits(const AVFrame* source,
                                           const double* hdr_to_nits,
                                           int convert_bt2020) {
  double peak = 0.0;
  int step_x = source->width / 160;
  int step_y = source->height / 90;
  if (step_x < 1) step_x = 1;
  if (step_y < 1) step_y = 1;
  for (int y = 0; y < source->height; y += step_y) {
    const uint16_t* source_row =
        (const uint16_t*)(source->data[0] + y * source->linesize[0]);
    for (int x = 0; x < source->width; x += step_x) {
      double r = hdr_to_nits[source_row[x * 3]];
      double g = hdr_to_nits[source_row[x * 3 + 1]];
      double b = hdr_to_nits[source_row[x * 3 + 2]];
      if (convert_bt2020) {
        convert_bt2020_to_bt709(&r, &g, &b);
      }
      double pixel_peak = fmax(r, fmax(g, b));
      if (pixel_peak > peak) {
        peak = pixel_peak;
      }
    }
  }
  return peak;
}

static double tone_map_knee_nits(double metadata_peak_nits,
                                 double frame_peak_nits) {
  double reference_peak = frame_peak_nits;
  if (metadata_peak_nits > 0.0) {
    reference_peak = reference_peak > 0.0 ? fmin(metadata_peak_nits, reference_peak)
                                          : metadata_peak_nits;
  }
  if (reference_peak <= 0.0) {
    return 300.0;
  }
  return clamp_double(reference_peak * 0.35, 180.0, 500.0);
}

static AVFrame* tone_map_hdr_rgb_frame(const AVFrame* source,
                                       const AVFrame* metadata_frame,
                                       const AVCodecContext* codec_context) {
  AVFrame* destination = allocate_rgb_frame(source->width, source->height);
  if (destination == NULL) {
    return NULL;
  }

  enum AVColorTransferCharacteristic transfer =
      transfer_for_frame(metadata_frame, codec_context);
  int convert_bt2020 =
      primaries_for_frame(metadata_frame, codec_context) == AVCOL_PRI_BT2020;
  double* hdr_to_nits = build_hdr_to_nits_table(transfer);
  uint8_t* srgb_encode_table = build_srgb_encode_table();
  if (hdr_to_nits == NULL || srgb_encode_table == NULL) {
    free(hdr_to_nits);
    free(srgb_encode_table);
    av_frame_free(&destination);
    return NULL;
  }
  double frame_peak =
      estimate_peak_luminance_nits(source, hdr_to_nits, convert_bt2020);
  double knee_nits =
      tone_map_knee_nits(hdr_metadata_peak_nits(metadata_frame), frame_peak);
  for (int y = 0; y < source->height; y++) {
    const uint16_t* source_row =
        (const uint16_t*)(source->data[0] + y * source->linesize[0]);
    uint8_t* destination_row = destination->data[0] + y * destination->linesize[0];
    for (int x = 0; x < source->width; x++) {
      double r = hdr_to_nits[source_row[x * 3]];
      double g = hdr_to_nits[source_row[x * 3 + 1]];
      double b = hdr_to_nits[source_row[x * 3 + 2]];
      if (convert_bt2020) {
        convert_bt2020_to_bt709(&r, &g, &b);
      }
      r = fmax(0.0, r);
      g = fmax(0.0, g);
      b = fmax(0.0, b);
      double luminance = r * 0.2126 + g * 0.7152 + b * 0.0722;
      double tone_mapped_luminance =
          tone_map_nits_to_sdr_with_knee(luminance, knee_nits);
      double scale = luminance > 1e-9 ? tone_mapped_luminance / luminance : 0.0;
      destination_row[x * 3] =
          srgb_encode_table[clamp_int((int)lrint(r * scale * 4096.0), 0, 4096)];
      destination_row[x * 3 + 1] =
          srgb_encode_table[clamp_int((int)lrint(g * scale * 4096.0), 0, 4096)];
      destination_row[x * 3 + 2] =
          srgb_encode_table[clamp_int((int)lrint(b * scale * 4096.0), 0, 4096)];
    }
  }
  free(hdr_to_nits);
  free(srgb_encode_table);
  return destination;
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
  long long requested_time_ms = 0;
  for (int i = 0; i < candidate_count; i++) {
    requested_time_ms = candidates[i] < 0 ? 0 : candidates[i];
    decoded =
        decode_frame_at(format_context, stream, codec_context, requested_time_ms,
                        decoded_frame);
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
  long long actual_time_ms = frame_time_ms(decoded_frame, stream);
  int long_edge = source_width > source_height ? source_width : source_height;
  double scale = long_edge > max_long_edge ? (double)max_long_edge / long_edge : 1.0;
  int out_width = source_width * scale;
  int out_height = source_height * scale;
  if (out_width < 1) out_width = 1;
  if (out_height < 1) out_height = 1;

  int use_hdr_tone_mapping = needs_hdr_tone_mapping(decoded_frame, codec_context);
  AVFrame* scaled_frame = use_hdr_tone_mapping
                              ? allocate_rgb48_frame(out_width, out_height)
                              : allocate_rgb_frame(out_width, out_height);
  if (scaled_frame == NULL) {
    av_frame_free(&decoded_frame);
    avcodec_free_context(&codec_context);
    avformat_close_input(&format_context);
    return error_json("outputFailed", "Could not allocate cover frame.");
  }

  enum AVPixelFormat source_format = codec_context->pix_fmt;
  if (use_hdr_tone_mapping) {
    source_format = (enum AVPixelFormat)decoded_frame->format;
    if (source_format == AV_PIX_FMT_NONE) {
      source_format = codec_context->pix_fmt;
    }
  }
  struct SwsContext* sws_context =
      sws_getContext(source_width, source_height, source_format, out_width,
                     out_height,
                     use_hdr_tone_mapping ? AV_PIX_FMT_RGB48LE : AV_PIX_FMT_RGB24,
                     SWS_BILINEAR, NULL, NULL, NULL);
  if (sws_context == NULL) {
    av_frame_free(&scaled_frame);
    av_frame_free(&decoded_frame);
    avcodec_free_context(&codec_context);
    avformat_close_input(&format_context);
    return error_json("outputFailed", "Could not create scaler.");
  }
  if (use_hdr_tone_mapping &&
      !configure_sws_colorspace(sws_context, decoded_frame, codec_context)) {
    sws_freeContext(sws_context);
    av_frame_free(&scaled_frame);
    av_frame_free(&decoded_frame);
    avcodec_free_context(&codec_context);
    avformat_close_input(&format_context);
    return error_json("outputFailed", "Could not configure HDR colorspace.");
  }

  int scaled_height =
      sws_scale(sws_context, (const uint8_t* const*)decoded_frame->data,
                decoded_frame->linesize, 0, source_height, scaled_frame->data,
                scaled_frame->linesize);
  if (scaled_height != out_height) {
    sws_freeContext(sws_context);
    av_frame_free(&scaled_frame);
    av_frame_free(&decoded_frame);
    avcodec_free_context(&codec_context);
    avformat_close_input(&format_context);
    return error_json("outputFailed", "Could not scale cover frame.");
  }

  AVFrame* tone_mapped_frame = NULL;
  AVFrame* rgb_frame = scaled_frame;
  if (use_hdr_tone_mapping) {
    tone_mapped_frame =
        tone_map_hdr_rgb_frame(scaled_frame, decoded_frame, codec_context);
    if (tone_mapped_frame == NULL) {
      sws_freeContext(sws_context);
      av_frame_free(&scaled_frame);
      av_frame_free(&decoded_frame);
      avcodec_free_context(&codec_context);
      avformat_close_input(&format_context);
      return error_json("outputFailed", "Could not tone-map HDR cover frame.");
    }
    rgb_frame = tone_mapped_frame;
  }

  AVFrame* cover_frame = rgb_frame;
  AVFrame* rotated_frame = rotate_rgb_frame(rgb_frame, rotation);
  if (rotation != 0) {
    if (rotated_frame == NULL) {
      sws_freeContext(sws_context);
      if (tone_mapped_frame != NULL) {
        av_frame_free(&tone_mapped_frame);
      }
      av_frame_free(&scaled_frame);
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
    if (tone_mapped_frame != NULL) {
      av_frame_free(&tone_mapped_frame);
    }
    av_frame_free(&scaled_frame);
    av_frame_free(&decoded_frame);
    avcodec_free_context(&codec_context);
    avformat_close_input(&format_context);
    return error_json("outputFailed", "Could not create cover file.");
  }

  int cover_width = cover_frame->width;
  int cover_height = cover_frame->height;
  sws_freeContext(sws_context);
  if (rotated_frame != NULL) {
    av_frame_free(&rotated_frame);
  }
  if (tone_mapped_frame != NULL) {
    av_frame_free(&tone_mapped_frame);
  }
  av_frame_free(&scaled_frame);
  av_frame_free(&decoded_frame);
  avcodec_free_context(&codec_context);
  avformat_close_input(&format_context);

  char result[1200];
  if (actual_time_ms >= 0) {
    snprintf(result, sizeof(result),
             "{\"coverPath\":\"%s\",\"width\":%d,\"height\":%d,"
             "\"requestedTimeMs\":%lld,\"actualTimeMs\":%lld}",
             output_path, cover_width, cover_height, requested_time_ms,
             actual_time_ms);
  } else {
    snprintf(result, sizeof(result),
             "{\"coverPath\":\"%s\",\"width\":%d,\"height\":%d,"
             "\"requestedTimeMs\":%lld,\"actualTimeMs\":null}",
             output_path, cover_width, cover_height, requested_time_ms);
  }
  return duplicate_string(result);
}

void lgpl_ffmpeg_flutter_free_string(char* value) {
  free(value);
}

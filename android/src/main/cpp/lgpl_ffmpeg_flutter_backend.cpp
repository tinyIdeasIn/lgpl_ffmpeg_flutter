#include <jni.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <sys/stat.h>
#include <sstream>
#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libavutil/display.h>
#include <libavutil/ffversion.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

namespace {

std::string EscapeJson(const std::string& value) {
  std::ostringstream out;
  for (char c : value) {
    switch (c) {
      case '"':
        out << "\\\"";
        break;
      case '\\':
        out << "\\\\";
        break;
      case '\n':
        out << "\\n";
        break;
      case '\r':
        out << "\\r";
        break;
      case '\t':
        out << "\\t";
        break;
      default:
        out << c;
        break;
    }
  }
  return out.str();
}

std::string ErrorJson(const char* code, const std::string& message) {
  return std::string("{\"errorCode\":\"") + code + "\",\"message\":\"" +
         EscapeJson(message) + "\"}";
}

std::string VersionString(unsigned version) {
  std::ostringstream out;
  out << AV_VERSION_MAJOR(version) << "." << AV_VERSION_MINOR(version) << "."
      << AV_VERSION_MICRO(version);
  return out.str();
}

std::string JStringToString(JNIEnv* env, jstring value) {
  if (value == nullptr) {
    return "";
  }
  const char* chars = env->GetStringUTFChars(value, nullptr);
  std::string result(chars == nullptr ? "" : chars);
  if (chars != nullptr) {
    env->ReleaseStringUTFChars(value, chars);
  }
  return result;
}

int64_t DurationMs(const AVFormatContext* format_context) {
  if (format_context->duration == AV_NOPTS_VALUE) {
    return 0;
  }
  return format_context->duration * 1000 / AV_TIME_BASE;
}

int RotationForStream(const AVStream* stream) {
  const AVPacketSideData* display_matrix =
      av_packet_side_data_get(stream->codecpar->coded_side_data,
                              stream->codecpar->nb_coded_side_data,
                              AV_PKT_DATA_DISPLAYMATRIX);
  if (display_matrix != nullptr) {
    const double angle = av_display_rotation_get(
        reinterpret_cast<const int32_t*>(display_matrix->data));
    if (!std::isnan(angle)) {
      int rotation = static_cast<int>(-angle);
      rotation %= 360;
      if (rotation < 0) {
        rotation += 360;
      }
      return rotation;
    }
  }

  AVDictionaryEntry* rotate =
      av_dict_get(stream->metadata, "rotate", nullptr, 0);
  return rotate == nullptr ? 0 : std::atoi(rotate->value);
}

int NormalizeRightAngle(int rotation) {
  rotation %= 360;
  if (rotation < 0) {
    rotation += 360;
  }
  return ((rotation + 45) / 90 * 90) % 360;
}

std::string MimeTypeForInput(const std::string& path,
                             const AVInputFormat* input_format) {
  if (input_format != nullptr && input_format->mime_type != nullptr) {
    return input_format->mime_type;
  }
  const auto dot = path.find_last_of('.');
  if (dot == std::string::npos) {
    return "";
  }
  std::string ext = path.substr(dot + 1);
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  if (ext == "mp4" || ext == "m4v") {
    return "video/mp4";
  }
  if (ext == "mov") {
    return "video/quicktime";
  }
  if (ext == "webm") {
    return "video/webm";
  }
  return "";
}

int64_t FileSizeBytes(const std::string& path) {
  struct stat stat_buffer {};
  if (stat(path.c_str(), &stat_buffer) != 0 || stat_buffer.st_size < 0) {
    return -1;
  }
  return static_cast<int64_t>(stat_buffer.st_size);
}

std::string CodecNameForStream(const AVStream* stream) {
  if (stream == nullptr || stream->codecpar == nullptr) {
    return "";
  }
  const char* name = avcodec_get_name(stream->codecpar->codec_id);
  return name == nullptr ? "" : name;
}

bool OpenVideo(const std::string& path,
               AVFormatContext** format_context,
               int* video_stream_index) {
  *format_context = nullptr;
  *video_stream_index = -1;

  if (avformat_open_input(format_context, path.c_str(), nullptr, nullptr) < 0) {
    return false;
  }

  if (avformat_find_stream_info(*format_context, nullptr) < 0) {
    avformat_close_input(format_context);
    return false;
  }

  const int index = av_find_best_stream(*format_context, AVMEDIA_TYPE_VIDEO, -1,
                                        -1, nullptr, 0);
  if (index < 0) {
    return true;
  }
  *video_stream_index = index;
  return true;
}

std::string ReadInfo(const std::string& path) {
  AVFormatContext* format_context = nullptr;
  int video_stream_index = -1;
  if (!OpenVideo(path, &format_context, &video_stream_index)) {
    return ErrorJson("openFailed", "Could not open video.");
  }
  if (video_stream_index < 0) {
    avformat_close_input(&format_context);
    return ErrorJson("noVideoStream", "No video stream found.");
  }

  AVStream* stream = format_context->streams[video_stream_index];
  AVCodecParameters* codecpar = stream->codecpar;
  const int audio_stream_index = av_find_best_stream(
      format_context, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
  AVStream* audio_stream = audio_stream_index >= 0
                               ? format_context->streams[audio_stream_index]
                               : nullptr;
  const int64_t file_size_bytes = FileSizeBytes(path);
  std::ostringstream json;
  json << "{";
  json << "\"durationMs\":" << DurationMs(format_context) << ",";
  json << "\"width\":" << codecpar->width << ",";
  json << "\"height\":" << codecpar->height << ",";
  json << "\"rotation\":" << RotationForStream(stream) << ",";
  if (format_context->bit_rate > 0) {
    json << "\"bitrate\":" << format_context->bit_rate << ",";
  } else {
    json << "\"bitrate\":null,";
  }
  json << "\"mimeType\":\""
       << EscapeJson(MimeTypeForInput(path, format_context->iformat)) << "\",";
  json << "\"formatName\":";
  if (format_context->iformat != nullptr &&
      format_context->iformat->name != nullptr) {
    json << "\"" << EscapeJson(format_context->iformat->name) << "\",";
  } else {
    json << "null,";
  }
  json << "\"videoCodec\":\"" << EscapeJson(CodecNameForStream(stream))
       << "\",";
  json << "\"audioCodec\":";
  const std::string audio_codec = CodecNameForStream(audio_stream);
  if (!audio_codec.empty()) {
    json << "\"" << EscapeJson(audio_codec) << "\",";
  } else {
    json << "null,";
  }
  json << "\"fileSizeBytes\":";
  if (file_size_bytes >= 0) {
    json << file_size_bytes;
  } else {
    json << "null";
  }
  json << "}";

  avformat_close_input(&format_context);
  return json.str();
}

std::string BackendInfo() {
  std::ostringstream json;
  json << "{";
  json << "\"ffmpegVersion\":\"" << EscapeJson(FFMPEG_VERSION) << "\",";
  json << "\"avformatVersion\":\""
       << EscapeJson(VersionString(avformat_version())) << "\",";
  json << "\"avcodecVersion\":\""
       << EscapeJson(VersionString(avcodec_version())) << "\",";
  json << "\"avutilVersion\":\"" << EscapeJson(VersionString(avutil_version()))
       << "\",";
  json << "\"configuration\":\""
       << EscapeJson(avformat_configuration()) << "\",";
  json << "\"license\":\"" << EscapeJson(avformat_license()) << "\",";
  json << "\"supportedInputFormats\":[\"mov\",\"matroska\",\"webm_dash_manifest\"],";
  json << "\"supportedVideoDecoders\":[\"h264\",\"hevc\",\"mpeg4\",\"vp8\",\"vp9\"],";
  json << "\"supportedAudioDecoders\":[\"aac\"],";
  json << "\"outputImageFormat\":\"png\"";
  json << "}";
  return json.str();
}

bool DecodeFrameAt(AVFormatContext* format_context,
                   AVStream* stream,
                   AVCodecContext* codec_context,
                   int64_t target_ms,
                   AVFrame* decoded_frame) {
  const int64_t timestamp =
      av_rescale_q(target_ms, AVRational{1, 1000}, stream->time_base);
  av_seek_frame(format_context, stream->index, timestamp, AVSEEK_FLAG_BACKWARD);
  avcodec_flush_buffers(codec_context);

  AVPacket* packet = av_packet_alloc();
  bool decoded = false;
  while (av_read_frame(format_context, packet) >= 0) {
    if (packet->stream_index == stream->index &&
        avcodec_send_packet(codec_context, packet) == 0) {
      while (avcodec_receive_frame(codec_context, decoded_frame) == 0) {
        decoded = true;
        break;
      }
    }
    av_packet_unref(packet);
    if (decoded) {
      break;
    }
  }
  av_packet_free(&packet);
  return decoded;
}

int64_t FrameTimeMs(const AVFrame* frame, const AVStream* stream) {
  if (frame == nullptr || stream == nullptr ||
      frame->best_effort_timestamp == AV_NOPTS_VALUE) {
    return -1;
  }
  return av_rescale_q(frame->best_effort_timestamp, stream->time_base,
                      AVRational{1, 1000});
}

bool WritePngFile(const std::string& output_path, const AVFrame* rgb_frame) {
  const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_PNG);
  if (codec == nullptr) {
    return false;
  }

  AVCodecContext* codec_context = avcodec_alloc_context3(codec);
  if (codec_context == nullptr) {
    return false;
  }
  codec_context->width = rgb_frame->width;
  codec_context->height = rgb_frame->height;
  codec_context->pix_fmt = AV_PIX_FMT_RGB24;
  codec_context->time_base = AVRational{1, 1};
  if (avcodec_open2(codec_context, codec, nullptr) < 0) {
    avcodec_free_context(&codec_context);
    return false;
  }

  AVPacket* packet = av_packet_alloc();
  if (packet == nullptr) {
    avcodec_free_context(&codec_context);
    return false;
  }

  bool success = false;
  if (avcodec_send_frame(codec_context, rgb_frame) == 0 &&
      avcodec_receive_packet(codec_context, packet) == 0) {
    FILE* file = std::fopen(output_path.c_str(), "wb");
    if (file != nullptr) {
      success = std::fwrite(packet->data, 1, packet->size, file) ==
                static_cast<size_t>(packet->size);
      std::fclose(file);
    }
  }

  av_packet_free(&packet);
  avcodec_free_context(&codec_context);
  return success;
}

AVFrame* AllocateRgbFrame(int width, int height) {
  AVFrame* frame = av_frame_alloc();
  if (frame == nullptr) {
    return nullptr;
  }
  frame->format = AV_PIX_FMT_RGB24;
  frame->width = width;
  frame->height = height;
  if (av_frame_get_buffer(frame, 32) < 0) {
    av_frame_free(&frame);
    return nullptr;
  }
  return frame;
}

void CopyRgbPixel(const AVFrame* source,
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

AVFrame* RotateRgbFrame(const AVFrame* source, int rotation) {
  const int normalized_rotation = NormalizeRightAngle(rotation);
  if (normalized_rotation == 0) {
    return nullptr;
  }

  const bool swaps_axes =
      normalized_rotation == 90 || normalized_rotation == 270;
  AVFrame* rotated = AllocateRgbFrame(swaps_axes ? source->height : source->width,
                                      swaps_axes ? source->width : source->height);
  if (rotated == nullptr) {
    return nullptr;
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
      CopyRgbPixel(source, rotated, x, y, destination_x, destination_y);
    }
  }

  return rotated;
}

std::string GenerateCover(const std::string& path,
                          const std::vector<int64_t>& preferred_times_ms,
                          int max_long_edge,
                          int quality,
                          const std::string& cache_dir) {
  (void)quality;
  AVFormatContext* format_context = nullptr;
  int video_stream_index = -1;
  if (!OpenVideo(path, &format_context, &video_stream_index)) {
    return ErrorJson("openFailed", "Could not open video.");
  }
  if (video_stream_index < 0) {
    avformat_close_input(&format_context);
    return ErrorJson("noVideoStream", "No video stream found.");
  }

  AVStream* stream = format_context->streams[video_stream_index];
  const int rotation = NormalizeRightAngle(RotationForStream(stream));
  const AVCodec* codec = avcodec_find_decoder(stream->codecpar->codec_id);
  if (codec == nullptr) {
    avformat_close_input(&format_context);
    return ErrorJson("decodeFailed", "No decoder found for video stream.");
  }

  AVCodecContext* codec_context = avcodec_alloc_context3(codec);
  avcodec_parameters_to_context(codec_context, stream->codecpar);
  if (avcodec_open2(codec_context, codec, nullptr) < 0) {
    avcodec_free_context(&codec_context);
    avformat_close_input(&format_context);
    return ErrorJson("decodeFailed", "Could not open video decoder.");
  }

  std::vector<int64_t> candidates = preferred_times_ms;
  const int64_t duration_ms = DurationMs(format_context);
  if (candidates.empty()) {
    candidates = {300, 1000};
    if (duration_ms > 0) {
      candidates.push_back(duration_ms / 10);
      candidates.push_back(duration_ms * 3 / 10);
    }
  }

  AVFrame* decoded_frame = av_frame_alloc();
  bool decoded = false;
  int64_t requested_time_ms = 0;
  for (int64_t candidate : candidates) {
    requested_time_ms = std::max<int64_t>(0, candidate);
    decoded = DecodeFrameAt(format_context, stream, codec_context,
                            requested_time_ms, decoded_frame);
    if (decoded) {
      break;
    }
  }

  if (!decoded) {
    av_frame_free(&decoded_frame);
    avcodec_free_context(&codec_context);
    avformat_close_input(&format_context);
    return "{\"coverPath\":null}";
  }

  const int source_width = decoded_frame->width;
  const int source_height = decoded_frame->height;
  const int64_t actual_time_ms = FrameTimeMs(decoded_frame, stream);
  const int long_edge = std::max(source_width, source_height);
  const double scale =
      long_edge > max_long_edge ? static_cast<double>(max_long_edge) / long_edge
                                : 1.0;
  const int out_width = std::max(1, static_cast<int>(source_width * scale));
  const int out_height = std::max(1, static_cast<int>(source_height * scale));

  AVFrame* rgb_frame = AllocateRgbFrame(out_width, out_height);
  if (rgb_frame == nullptr) {
    av_frame_free(&decoded_frame);
    avcodec_free_context(&codec_context);
    avformat_close_input(&format_context);
    return ErrorJson("outputFailed", "Could not allocate cover frame.");
  }

  SwsContext* sws_context = sws_getContext(
      source_width, source_height, codec_context->pix_fmt, out_width, out_height,
      AV_PIX_FMT_RGB24, SWS_BILINEAR, nullptr, nullptr, nullptr);
  if (sws_context == nullptr) {
    av_frame_free(&rgb_frame);
    av_frame_free(&decoded_frame);
    avcodec_free_context(&codec_context);
    avformat_close_input(&format_context);
    return ErrorJson("outputFailed", "Could not create scaler.");
  }

  sws_scale(sws_context, decoded_frame->data, decoded_frame->linesize, 0,
            source_height, rgb_frame->data, rgb_frame->linesize);

  AVFrame* cover_frame = rgb_frame;
  AVFrame* rotated_frame = RotateRgbFrame(rgb_frame, rotation);
  if (rotation != 0) {
    if (rotated_frame == nullptr) {
      sws_freeContext(sws_context);
      av_frame_free(&rgb_frame);
      av_frame_free(&decoded_frame);
      avcodec_free_context(&codec_context);
      avformat_close_input(&format_context);
      return ErrorJson("outputFailed", "Could not rotate cover frame.");
    }
    cover_frame = rotated_frame;
  }

  std::ostringstream output_path;
  output_path << cache_dir << "/lgpl_ffmpeg_cover_" << std::time(nullptr)
              << "_" << std::rand() << ".png";

  if (!WritePngFile(output_path.str(), cover_frame)) {
    sws_freeContext(sws_context);
    if (rotated_frame != nullptr) {
      av_frame_free(&rotated_frame);
    }
    av_frame_free(&rgb_frame);
    av_frame_free(&decoded_frame);
    avcodec_free_context(&codec_context);
    avformat_close_input(&format_context);
    return ErrorJson("outputFailed", "Could not create cover file.");
  }

  sws_freeContext(sws_context);
  if (rotated_frame != nullptr) {
    av_frame_free(&rotated_frame);
  }
  av_frame_free(&rgb_frame);
  av_frame_free(&decoded_frame);
  avcodec_free_context(&codec_context);
  avformat_close_input(&format_context);

  std::ostringstream result;
  result << "{\"coverPath\":\"" << EscapeJson(output_path.str()) << "\",";
  result << "\"width\":" << cover_frame->width << ",";
  result << "\"height\":" << cover_frame->height << ",";
  result << "\"requestedTimeMs\":" << requested_time_ms << ",";
  result << "\"actualTimeMs\":";
  if (actual_time_ms >= 0) {
    result << actual_time_ms;
  } else {
    result << "null";
  }
  result << "}";
  return result.str();
}

}  // namespace

extern "C" const char* lgpl_ffmpeg_flutter_backend_status(void) {
  return "LGPL FFmpeg backend linked.";
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_addcn_lgpl_1ffmpeg_1flutter_LgplFfmpegFlutterPlugin_nativeReadInfo(
    JNIEnv* env,
    jobject,
    jstring video_path) {
  const std::string result = ReadInfo(JStringToString(env, video_path));
  return env->NewStringUTF(result.c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_addcn_lgpl_1ffmpeg_1flutter_LgplFfmpegFlutterPlugin_nativeBackendInfo(
    JNIEnv* env,
    jobject) {
  const std::string result = BackendInfo();
  return env->NewStringUTF(result.c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_addcn_lgpl_1ffmpeg_1flutter_LgplFfmpegFlutterPlugin_nativeGenerateCover(
    JNIEnv* env,
    jobject,
    jstring video_path,
    jlongArray preferred_times_ms,
    jint max_long_edge,
    jint quality,
    jstring cache_dir) {
  std::vector<int64_t> preferred_times;
  if (preferred_times_ms != nullptr) {
    const jsize count = env->GetArrayLength(preferred_times_ms);
    std::vector<jlong> values(count);
    env->GetLongArrayRegion(preferred_times_ms, 0, count, values.data());
    for (jlong value : values) {
      preferred_times.push_back(static_cast<int64_t>(value));
    }
  }
  const std::string result = GenerateCover(
      JStringToString(env, video_path), preferred_times, max_long_edge, quality,
      JStringToString(env, cache_dir));
  return env->NewStringUTF(result.c_str());
}

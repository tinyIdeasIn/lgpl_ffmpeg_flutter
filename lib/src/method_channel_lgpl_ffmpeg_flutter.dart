import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

import 'cover_image.dart';
import 'ffmpeg_backend_info.dart';
import 'lgpl_ffmpeg_flutter_platform.dart';
import 'video_info.dart';
import 'video_process_error.dart';

/// Method channel implementation of [LgplFfmpegFlutterPlatform].
class MethodChannelLgplFfmpegFlutter extends LgplFfmpegFlutterPlatform {
  /// Channel used to call the native Android and iOS implementations.
  @visibleForTesting
  final methodChannel = const MethodChannel('lgpl_ffmpeg_flutter');

  @override
  Future<VideoInfo> readInfo({required String videoPath}) async {
    _validateVideoPath(videoPath);

    try {
      final result = await methodChannel.invokeMapMethod<Object?, Object?>(
        'readInfo',
        <String, Object?>{'videoPath': videoPath},
      );
      if (result == null) {
        throw const FormatException('readInfo returned null.');
      }
      return VideoInfo.fromMap(result);
    } on PlatformException catch (exception) {
      throw VideoProcessException.fromPlatformException(exception);
    }
  }

  @override
  Future<FfmpegBackendInfo> backendInfo() async {
    try {
      final result = await methodChannel.invokeMapMethod<Object?, Object?>(
        'backendInfo',
      );
      if (result == null) {
        throw const FormatException('backendInfo returned null.');
      }
      return FfmpegBackendInfo.fromMap(result);
    } on PlatformException catch (exception) {
      throw VideoProcessException.fromPlatformException(exception);
    }
  }

  @override
  Future<String?> generateCover({
    required String videoPath,
    List<Duration>? preferredTimes,
    int maxLongEdge = 1920,
    int quality = 95,
  }) async {
    final cover = await generateCoverImage(
      videoPath: videoPath,
      preferredTimes: preferredTimes,
      maxLongEdge: maxLongEdge,
      quality: quality,
    );
    return cover?.path;
  }

  @override
  Future<CoverImage?> generateCoverImage({
    required String videoPath,
    List<Duration>? preferredTimes,
    int maxLongEdge = 1920,
    int quality = 95,
  }) async {
    _validateVideoPath(videoPath);
    if (maxLongEdge <= 0) {
      throw const VideoProcessException(
        code: VideoProcessErrorCode.invalidPath,
        message: 'maxLongEdge must be greater than 0.',
      );
    }
    if (quality < 1 || quality > 100) {
      throw const VideoProcessException(
        code: VideoProcessErrorCode.invalidPath,
        message: 'quality must be between 1 and 100.',
      );
    }

    try {
      final result = await methodChannel
          .invokeMapMethod<Object?, Object?>('generateCover', <String, Object?>{
            'videoPath': videoPath,
            'preferredTimesMs': preferredTimes
                ?.map((duration) => duration.inMilliseconds)
                .toList(),
            'maxLongEdge': maxLongEdge,
            'quality': quality,
          });
      if (result == null || result['coverPath'] == null) {
        return null;
      }
      return CoverImage.fromMap(result);
    } on PlatformException catch (exception) {
      throw VideoProcessException.fromPlatformException(exception);
    }
  }

  @override
  Future<CoverImage?> extractFrame({
    required String videoPath,
    required Duration time,
    int maxLongEdge = 1920,
    int quality = 95,
  }) async {
    _validateVideoPath(videoPath);
    if (maxLongEdge <= 0) {
      throw const VideoProcessException(
        code: VideoProcessErrorCode.invalidPath,
        message: 'maxLongEdge must be greater than 0.',
      );
    }
    if (quality < 1 || quality > 100) {
      throw const VideoProcessException(
        code: VideoProcessErrorCode.invalidPath,
        message: 'quality must be between 1 and 100.',
      );
    }

    try {
      final result = await methodChannel
          .invokeMapMethod<Object?, Object?>('extractFrame', <String, Object?>{
            'videoPath': videoPath,
            'timeMs': time.inMilliseconds,
            'maxLongEdge': maxLongEdge,
            'quality': quality,
          });
      if (result == null || result['coverPath'] == null) {
        return null;
      }
      return CoverImage.fromMap(result);
    } on PlatformException catch (exception) {
      throw VideoProcessException.fromPlatformException(exception);
    }
  }

  @override
  Future<int> deleteGeneratedFiles() async {
    try {
      final count = await methodChannel.invokeMethod<int>(
        'deleteGeneratedFiles',
      );
      return count ?? 0;
    } on PlatformException catch (exception) {
      throw VideoProcessException.fromPlatformException(exception);
    }
  }

  void _validateVideoPath(String videoPath) {
    if (videoPath.trim().isEmpty) {
      throw const VideoProcessException(
        code: VideoProcessErrorCode.invalidPath,
        message: 'videoPath must not be empty.',
      );
    }
  }
}

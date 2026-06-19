import 'package:plugin_platform_interface/plugin_platform_interface.dart';

import 'cover_image.dart';
import 'ffmpeg_backend_info.dart';
import 'method_channel_lgpl_ffmpeg_flutter.dart';
import 'video_info.dart';

/// Platform interface used by platform-specific FFmpeg backends.
abstract class LgplFfmpegFlutterPlatform extends PlatformInterface {
  /// Creates a platform interface instance.
  LgplFfmpegFlutterPlatform() : super(token: _token);

  static final Object _token = Object();

  static LgplFfmpegFlutterPlatform _instance = MethodChannelLgplFfmpegFlutter();

  /// The active platform implementation.
  static LgplFfmpegFlutterPlatform get instance => _instance;

  /// Replaces the active platform implementation.
  static set instance(LgplFfmpegFlutterPlatform instance) {
    PlatformInterface.verifyToken(instance, _token);
    _instance = instance;
  }

  /// Reads video metadata from [videoPath].
  Future<VideoInfo> readInfo({required String videoPath}) {
    throw UnimplementedError('readInfo() has not been implemented.');
  }

  /// Reads runtime diagnostics from the native FFmpeg backend.
  Future<FfmpegBackendInfo> backendInfo() {
    throw UnimplementedError('backendInfo() has not been implemented.');
  }

  /// Generates a cover image for [videoPath].
  Future<String?> generateCover({
    required String videoPath,
    List<Duration>? preferredTimes,
    int maxLongEdge = 1920,
    int quality = 95,
  }) {
    throw UnimplementedError('generateCover() has not been implemented.');
  }

  /// Generates a cover image for [videoPath] and returns output metadata.
  Future<CoverImage?> generateCoverImage({
    required String videoPath,
    List<Duration>? preferredTimes,
    int maxLongEdge = 1920,
    int quality = 95,
  }) {
    throw UnimplementedError('generateCoverImage() has not been implemented.');
  }

  /// Extracts a frame near [time] and returns output metadata.
  Future<CoverImage?> extractFrame({
    required String videoPath,
    required Duration time,
    int maxLongEdge = 1920,
    int quality = 95,
  }) {
    throw UnimplementedError('extractFrame() has not been implemented.');
  }

  /// Deletes generated plugin files from the platform cache.
  Future<int> deleteGeneratedFiles() {
    throw UnimplementedError(
      'deleteGeneratedFiles() has not been implemented.',
    );
  }
}

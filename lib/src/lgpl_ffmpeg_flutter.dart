import 'lgpl_ffmpeg_flutter_platform.dart';
import 'video_info.dart';

/// Entry point for video metadata and cover extraction helpers.
class LgplFfmpegFlutter {
  const LgplFfmpegFlutter._();

  /// Reads basic media metadata from the video file at [videoPath].
  ///
  /// Throws a `VideoProcessException` when the native backend cannot open or
  /// inspect the file.
  static Future<VideoInfo> readInfo({required String videoPath}) {
    return LgplFfmpegFlutterPlatform.instance.readInfo(videoPath: videoPath);
  }

  /// Extracts a cover image from the video file at [videoPath].
  ///
  /// [preferredTimes] lets callers provide candidate timestamps. The native
  /// backend chooses the first timestamp that can produce a frame. [maxLongEdge]
  /// limits the generated image size and [quality] controls JPEG quality from
  /// 1 to 100. Returns the generated cover path, or `null` if no cover was
  /// produced.
  static Future<String?> generateCover({
    required String videoPath,
    List<Duration>? preferredTimes,
    int maxLongEdge = 1920,
    int quality = 95,
  }) {
    return LgplFfmpegFlutterPlatform.instance.generateCover(
      videoPath: videoPath,
      preferredTimes: preferredTimes,
      maxLongEdge: maxLongEdge,
      quality: quality,
    );
  }
}

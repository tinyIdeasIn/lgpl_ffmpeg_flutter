import 'lgpl_ffmpeg_flutter_platform.dart';
import 'video_info.dart';

class LgplFfmpegFlutter {
  const LgplFfmpegFlutter._();

  static Future<VideoInfo> readInfo({required String videoPath}) {
    return LgplFfmpegFlutterPlatform.instance.readInfo(videoPath: videoPath);
  }

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

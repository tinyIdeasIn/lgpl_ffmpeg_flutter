import 'package:plugin_platform_interface/plugin_platform_interface.dart';

import 'method_channel_lgpl_ffmpeg_flutter.dart';
import 'video_info.dart';

abstract class LgplFfmpegFlutterPlatform extends PlatformInterface {
  LgplFfmpegFlutterPlatform() : super(token: _token);

  static final Object _token = Object();

  static LgplFfmpegFlutterPlatform _instance = MethodChannelLgplFfmpegFlutter();

  static LgplFfmpegFlutterPlatform get instance => _instance;

  static set instance(LgplFfmpegFlutterPlatform instance) {
    PlatformInterface.verifyToken(instance, _token);
    _instance = instance;
  }

  Future<VideoInfo> readInfo({required String videoPath}) {
    throw UnimplementedError('readInfo() has not been implemented.');
  }

  Future<String?> generateCover({
    required String videoPath,
    List<Duration>? preferredTimes,
    int maxLongEdge = 1920,
    int quality = 95,
  }) {
    throw UnimplementedError('generateCover() has not been implemented.');
  }
}

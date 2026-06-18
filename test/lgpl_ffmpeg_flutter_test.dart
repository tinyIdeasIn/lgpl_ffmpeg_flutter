import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:lgpl_ffmpeg_flutter/lgpl_ffmpeg_flutter.dart';
import 'package:plugin_platform_interface/plugin_platform_interface.dart';

class FakeLgplFfmpegFlutterPlatform
    with MockPlatformInterfaceMixin
    implements LgplFfmpegFlutterPlatform {
  @override
  Future<VideoInfo> readInfo({required String videoPath}) async {
    return const VideoInfo(
      duration: Duration(milliseconds: 12345),
      width: 1920,
      height: 1080,
      rotation: 0,
      bitrate: 3000000,
      mimeType: 'video/mp4',
    );
  }

  @override
  Future<String?> generateCover({
    required String videoPath,
    List<Duration>? preferredTimes,
    int maxLongEdge = 1920,
    int quality = 95,
  }) async {
    return null;
  }
}

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  test('method channel is the default platform implementation', () {
    expect(
      LgplFfmpegFlutterPlatform.instance,
      isInstanceOf<MethodChannelLgplFfmpegFlutter>(),
    );
  });

  test('public API delegates readInfo to platform implementation', () async {
    LgplFfmpegFlutterPlatform.instance = FakeLgplFfmpegFlutterPlatform();

    final info = await LgplFfmpegFlutter.readInfo(videoPath: '/tmp/video.mp4');

    expect(info.duration, const Duration(milliseconds: 12345));
    expect(info.width, 1920);
    expect(info.height, 1080);
    expect(info.bitrate, 3000000);
    expect(info.mimeType, 'video/mp4');
  });

  test('public API allows null cover path', () async {
    LgplFfmpegFlutterPlatform.instance = FakeLgplFfmpegFlutterPlatform();

    final cover = await LgplFfmpegFlutter.generateCover(
      videoPath: '/tmp/video.mp4',
    );

    expect(cover, isNull);
  });

  test('VideoInfo parses method channel map', () {
    final info = VideoInfo.fromMap(<Object?, Object?>{
      'durationMs': 1200,
      'width': 640,
      'height': 480,
      'rotation': 90,
      'bitrate': 1000000,
      'mimeType': 'video/quicktime',
    });

    expect(info.duration, const Duration(milliseconds: 1200));
    expect(info.width, 640);
    expect(info.height, 480);
    expect(info.rotation, 90);
    expect(info.bitrate, 1000000);
    expect(info.mimeType, 'video/quicktime');
  });

  test('VideoProcessException maps PlatformException code', () {
    final exception = VideoProcessException.fromPlatformException(
      PlatformException(
        code: 'openFailed',
        message: 'Cannot open video.',
        details: <String, Object?>{'path': '/tmp/video.mp4'},
      ),
    );

    expect(exception.code, VideoProcessErrorCode.openFailed);
    expect(exception.message, 'Cannot open video.');
    expect(exception.details, isA<Map<String, Object?>>());
  });
}

import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:lgpl_ffmpeg_flutter/lgpl_ffmpeg_flutter.dart';
import 'package:plugin_platform_interface/plugin_platform_interface.dart';

class FakeLgplFfmpegFlutterPlatform
    with MockPlatformInterfaceMixin
    implements LgplFfmpegFlutterPlatform {
  @override
  Future<FfmpegBackendInfo> backendInfo() async {
    return const FfmpegBackendInfo(
      ffmpegVersion: '8.0.1',
      avformatVersion: '62.3.100',
      avcodecVersion: '62.11.100',
      avutilVersion: '60.8.100',
      configuration: '--disable-gpl --disable-nonfree --enable-shared',
      license: 'LGPL version 2.1 or later',
    );
  }

  @override
  Future<VideoInfo> readInfo({required String videoPath}) async {
    return const VideoInfo(
      duration: Duration(milliseconds: 12345),
      width: 1920,
      height: 1080,
      rotation: 0,
      bitrate: 3000000,
      mimeType: 'video/mp4',
      formatName: 'mov,mp4,m4a,3gp,3g2,mj2',
      videoCodec: 'h264',
      audioCodec: 'aac',
      fileSizeBytes: 1234567,
    );
  }

  @override
  Future<String?> generateCover({
    required String videoPath,
    List<Duration>? preferredTimes,
    int maxLongEdge = 1920,
    int quality = 95,
  }) async {
    return '/tmp/lgpl_ffmpeg_cover.png';
  }

  @override
  Future<CoverImage?> generateCoverImage({
    required String videoPath,
    List<Duration>? preferredTimes,
    int maxLongEdge = 1920,
    int quality = 95,
  }) async {
    return const CoverImage(
      path: '/tmp/lgpl_ffmpeg_cover.png',
      width: 1280,
      height: 720,
      requestedTime: Duration(seconds: 1),
      actualTime: Duration(milliseconds: 960),
    );
  }

  @override
  Future<CoverImage?> extractFrame({
    required String videoPath,
    required Duration time,
    int maxLongEdge = 1920,
    int quality = 95,
  }) async {
    return CoverImage(
      path: '/tmp/lgpl_ffmpeg_frame.png',
      width: 640,
      height: 360,
      requestedTime: time,
      actualTime: const Duration(milliseconds: 2040),
    );
  }

  @override
  Future<int> deleteGeneratedFiles() async {
    return 2;
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
    expect(info.formatName, 'mov,mp4,m4a,3gp,3g2,mj2');
    expect(info.videoCodec, 'h264');
    expect(info.audioCodec, 'aac');
    expect(info.fileSizeBytes, 1234567);
  });

  test('public API delegates backendInfo to platform implementation', () async {
    LgplFfmpegFlutterPlatform.instance = FakeLgplFfmpegFlutterPlatform();

    final info = await LgplFfmpegFlutter.backendInfo();

    expect(info.ffmpegVersion, '8.0.1');
    expect(info.configuration, contains('--disable-gpl'));
    expect(info.license, contains('LGPL'));
  });

  test('public API returns generated png cover path', () async {
    LgplFfmpegFlutterPlatform.instance = FakeLgplFfmpegFlutterPlatform();

    final cover = await LgplFfmpegFlutter.generateCover(
      videoPath: '/tmp/video.mp4',
    );

    expect(cover, endsWith('.png'));
  });

  test('public API returns structured generated cover image', () async {
    LgplFfmpegFlutterPlatform.instance = FakeLgplFfmpegFlutterPlatform();

    final cover = await LgplFfmpegFlutter.generateCoverImage(
      videoPath: '/tmp/video.mp4',
    );

    expect(cover?.path, endsWith('.png'));
    expect(cover?.width, 1280);
    expect(cover?.height, 720);
    expect(cover?.requestedTime, const Duration(seconds: 1));
    expect(cover?.actualTime, const Duration(milliseconds: 960));
  });

  test(
    'public API delegates extractFrame to platform implementation',
    () async {
      LgplFfmpegFlutterPlatform.instance = FakeLgplFfmpegFlutterPlatform();

      final frame = await LgplFfmpegFlutter.extractFrame(
        videoPath: '/tmp/video.mp4',
        time: const Duration(seconds: 2),
      );

      expect(frame?.path, endsWith('.png'));
      expect(frame?.width, 640);
      expect(frame?.height, 360);
      expect(frame?.requestedTime, const Duration(seconds: 2));
      expect(frame?.actualTime, const Duration(milliseconds: 2040));
    },
  );

  test('public API delegates deleteGeneratedFiles to platform', () async {
    LgplFfmpegFlutterPlatform.instance = FakeLgplFfmpegFlutterPlatform();

    final count = await LgplFfmpegFlutter.deleteGeneratedFiles();

    expect(count, 2);
  });

  test('CoverImage parses method channel map', () {
    final cover = CoverImage.fromMap(<Object?, Object?>{
      'coverPath': '/tmp/lgpl_ffmpeg_cover.png',
      'width': 640,
      'height': 360,
      'requestedTimeMs': 1000,
      'actualTimeMs': 960,
    });

    expect(cover.path, '/tmp/lgpl_ffmpeg_cover.png');
    expect(cover.width, 640);
    expect(cover.height, 360);
    expect(cover.requestedTime, const Duration(seconds: 1));
    expect(cover.actualTime, const Duration(milliseconds: 960));
  });

  test('VideoInfo parses method channel map', () {
    final info = VideoInfo.fromMap(<Object?, Object?>{
      'durationMs': 1200,
      'width': 640,
      'height': 480,
      'rotation': 90,
      'bitrate': 1000000,
      'mimeType': 'video/quicktime',
      'formatName': 'mov,mp4,m4a,3gp,3g2,mj2',
      'videoCodec': 'h264',
      'audioCodec': 'aac',
      'fileSizeBytes': 345678,
    });

    expect(info.duration, const Duration(milliseconds: 1200));
    expect(info.width, 640);
    expect(info.height, 480);
    expect(info.rotation, 90);
    expect(info.bitrate, 1000000);
    expect(info.mimeType, 'video/quicktime');
    expect(info.formatName, 'mov,mp4,m4a,3gp,3g2,mj2');
    expect(info.videoCodec, 'h264');
    expect(info.audioCodec, 'aac');
    expect(info.fileSizeBytes, 345678);
  });

  test('VideoInfo keeps new metadata fields optional', () {
    final info = VideoInfo.fromMap(<Object?, Object?>{'durationMs': 1200});

    expect(info.formatName, isNull);
    expect(info.videoCodec, isNull);
    expect(info.audioCodec, isNull);
    expect(info.fileSizeBytes, isNull);
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

  test('FfmpegBackendInfo parses method channel map', () {
    final info = FfmpegBackendInfo.fromMap(<Object?, Object?>{
      'ffmpegVersion': '8.0.1',
      'avformatVersion': '62.3.100',
      'avcodecVersion': '62.11.100',
      'avutilVersion': '60.8.100',
      'configuration': '--disable-gpl --disable-nonfree',
      'license': 'LGPL version 2.1 or later',
    });

    expect(info.ffmpegVersion, '8.0.1');
    expect(info.avformatVersion, '62.3.100');
    expect(info.avcodecVersion, '62.11.100');
    expect(info.avutilVersion, '60.8.100');
    expect(info.configuration, contains('--disable-nonfree'));
    expect(info.license, contains('LGPL'));
  });
}

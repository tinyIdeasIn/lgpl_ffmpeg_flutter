import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:lgpl_ffmpeg_flutter/lgpl_ffmpeg_flutter.dart';

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  final platform = MethodChannelLgplFfmpegFlutter();
  const channel = MethodChannel('lgpl_ffmpeg_flutter');
  final calls = <MethodCall>[];

  setUp(() {
    calls.clear();
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(channel, (methodCall) async {
          calls.add(methodCall);
          switch (methodCall.method) {
            case 'readInfo':
              return <String, Object?>{
                'durationMs': 12345,
                'width': 1920,
                'height': 1080,
                'rotation': 0,
                'bitrate': 3000000,
                'mimeType': 'video/mp4',
              };
            case 'generateCover':
              return <String, Object?>{
                'coverPath': '/tmp/lgpl_ffmpeg_cover.png',
              };
          }
          throw PlatformException(code: 'unknown');
        });
  });

  tearDown(() {
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(channel, null);
  });

  test(
    'readInfo sends only the controlled method and expected arguments',
    () async {
      final info = await platform.readInfo(videoPath: '/tmp/video.mp4');

      expect(info.duration, const Duration(milliseconds: 12345));
      expect(calls.single.method, 'readInfo');
      expect(calls.single.arguments, <String, Object?>{
        'videoPath': '/tmp/video.mp4',
      });
    },
  );

  test('generateCover serializes preferred times as milliseconds', () async {
    final cover = await platform.generateCover(
      videoPath: '/tmp/video.mp4',
      preferredTimes: const [Duration(milliseconds: 300), Duration(seconds: 1)],
      maxLongEdge: 1280,
      quality: 90,
    );

    expect(cover, endsWith('.png'));
    expect(calls.single.method, 'generateCover');
    expect(calls.single.arguments, <String, Object?>{
      'videoPath': '/tmp/video.mp4',
      'preferredTimesMs': <int>[300, 1000],
      'maxLongEdge': 1280,
      'quality': 90,
    });
  });

  test('platform exceptions are mapped to VideoProcessException', () async {
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(channel, (_) async {
          throw PlatformException(
            code: 'ffmpegUnavailable',
            message: 'FFmpeg is not linked.',
          );
        });

    expect(
      () => platform.readInfo(videoPath: '/tmp/video.mp4'),
      throwsA(
        isA<VideoProcessException>().having(
          (exception) => exception.code,
          'code',
          VideoProcessErrorCode.ffmpegUnavailable,
        ),
      ),
    );
  });
}

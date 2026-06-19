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
                'formatName': 'mov,mp4,m4a,3gp,3g2,mj2',
                'videoCodec': 'h264',
                'audioCodec': 'aac',
                'fileSizeBytes': 1234567,
              };
            case 'backendInfo':
              return <String, Object?>{
                'ffmpegVersion': '8.0.1',
                'avformatVersion': '62.3.100',
                'avcodecVersion': '62.11.100',
                'avutilVersion': '60.8.100',
                'configuration': '--disable-gpl --disable-nonfree',
                'license': 'LGPL version 2.1 or later',
                'supportedInputFormats': <String>['mov', 'matroska'],
                'supportedVideoDecoders': <String>['h264', 'hevc'],
                'supportedAudioDecoders': <String>['aac'],
                'outputImageFormat': 'png',
              };
            case 'generateCover':
              return <String, Object?>{
                'coverPath': '/tmp/lgpl_ffmpeg_cover.png',
                'width': 1280,
                'height': 720,
                'requestedTimeMs': 300,
                'actualTimeMs': 320,
              };
            case 'extractFrame':
              return <String, Object?>{
                'coverPath': '/tmp/lgpl_ffmpeg_frame.png',
                'width': 640,
                'height': 360,
                'requestedTimeMs': 2000,
                'actualTimeMs': 2040,
              };
            case 'deleteGeneratedFiles':
              return 2;
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
      expect(info.videoCodec, 'h264');
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

  test('generateCoverImage parses structured cover result', () async {
    final cover = await platform.generateCoverImage(
      videoPath: '/tmp/video.mp4',
      preferredTimes: const [Duration(milliseconds: 300)],
      maxLongEdge: 1280,
      quality: 90,
    );

    expect(cover?.path, '/tmp/lgpl_ffmpeg_cover.png');
    expect(cover?.width, 1280);
    expect(cover?.height, 720);
    expect(cover?.requestedTime, const Duration(milliseconds: 300));
    expect(cover?.actualTime, const Duration(milliseconds: 320));
    expect(calls.single.method, 'generateCover');
  });

  test('extractFrame sends requested time as milliseconds', () async {
    final frame = await platform.extractFrame(
      videoPath: '/tmp/video.mp4',
      time: const Duration(seconds: 2),
      maxLongEdge: 640,
      quality: 90,
    );

    expect(frame?.path, '/tmp/lgpl_ffmpeg_frame.png');
    expect(frame?.requestedTime, const Duration(seconds: 2));
    expect(frame?.actualTime, const Duration(milliseconds: 2040));
    expect(calls.single.method, 'extractFrame');
    expect(calls.single.arguments, <String, Object?>{
      'videoPath': '/tmp/video.mp4',
      'timeMs': 2000,
      'maxLongEdge': 640,
      'quality': 90,
    });
  });

  test('backendInfo sends the controlled diagnostic method', () async {
    final info = await platform.backendInfo();

    expect(info.ffmpegVersion, '8.0.1');
    expect(info.license, contains('LGPL'));
    expect(info.supportedInputFormats, contains('mov'));
    expect(info.supportedVideoDecoders, contains('h264'));
    expect(info.supportedAudioDecoders, contains('aac'));
    expect(calls.single.method, 'backendInfo');
    expect(calls.single.arguments, isNull);
  });

  test('deleteGeneratedFiles sends the controlled cleanup method', () async {
    final count = await platform.deleteGeneratedFiles();

    expect(count, 2);
    expect(calls.single.method, 'deleteGeneratedFiles');
    expect(calls.single.arguments, isNull);
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

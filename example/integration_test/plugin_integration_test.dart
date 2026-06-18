// This is a basic Flutter integration test.
//
// Since integration tests run in a full Flutter application, they can interact
// with the host side of a plugin implementation, unlike Dart unit tests.
//
// For more information about Flutter integration tests, please see
// https://flutter.dev/to/integration-testing

import 'package:integration_test/integration_test.dart';
import 'package:flutter_test/flutter_test.dart';

import 'package:lgpl_ffmpeg_flutter/lgpl_ffmpeg_flutter.dart';

void main() {
  IntegrationTestWidgetsFlutterBinding.ensureInitialized();

  testWidgets('native backend reports unavailable until FFmpeg is linked', (
    WidgetTester tester,
  ) async {
    expect(
      () => LgplFfmpegFlutter.readInfo(videoPath: '/tmp/video.mp4'),
      throwsA(isA<VideoProcessException>()),
    );
  });
}

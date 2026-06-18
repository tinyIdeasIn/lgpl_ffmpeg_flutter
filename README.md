# lgpl_ffmpeg_flutter

[![pub package](https://img.shields.io/badge/pub-0.0.2-blue.svg)](https://pub.dev/packages/lgpl_ffmpeg_flutter)

Controlled Flutter plugin for reading local video metadata and extracting a
cover frame through LGPL FFmpeg dynamic libraries.

This package intentionally exposes a small API surface. It does not provide
arbitrary FFmpeg command execution, transcoding, compression, HLS processing,
network input, or GPL/nonfree codec builds.

## Platform Support

| Platform | Status | Notes |
| --- | --- | --- |
| Android | Supported | `arm64-v8a` only, minSdk 24 |
| iOS | Supported | arm64 device and arm64 simulator, iOS 13+ |
| macOS, Windows, Linux, Web | Not supported | No platform implementation is bundled |

Android apps should keep `arm64-v8a` in their ABI filters unless they rebuild
and add FFmpeg dynamic libraries for additional ABIs.

## Installation

```sh
flutter pub add lgpl_ffmpeg_flutter
```

## Usage

```dart
import 'package:lgpl_ffmpeg_flutter/lgpl_ffmpeg_flutter.dart';

final info = await LgplFfmpegFlutter.readInfo(videoPath: video.path);

final cover = await LgplFfmpegFlutter.generateCover(
  videoPath: video.path,
  preferredTimes: const [Duration(seconds: 1), Duration(seconds: 3)],
  maxLongEdge: 1280,
  quality: 90,
);

print(info.duration);
print(cover);
```

`readInfo` returns a `VideoInfo` value with the following fields:

- `duration`: video duration.
- `width` and `height`: encoded video dimensions when available.
- `rotation`: display rotation in degrees, defaulting to `0`.
- `bitrate`: stream bitrate when available.
- `mimeType`: container or video MIME type when available.

`generateCover` writes a `.png` image file in the platform cache directory.

`generateCover` returns `null` when the video can be opened but no candidate
time produces a frame. If native FFmpeg libraries are unavailable, the plugin
throws a structured `ffmpegUnavailable` error.

`generateCover` accepts these options:

- `preferredTimes`: candidate timestamps to try in order. When omitted, the
  native implementation chooses fallback timestamps.
- `maxLongEdge`: maximum output width or height after scaling. The value must
  be greater than `0`; the default is `1920`.
- `quality`: reserved output quality value from `1` to `100`; the default is
  `95`.

Errors from the platform layer are surfaced as `VideoProcessException`.
Known error codes include:

- `invalidPath`: the input path is empty or otherwise invalid. The Dart API
  also uses this code for `maxLongEdge <= 0` or `quality` outside `1..100`
  before invoking the platform layer.
- `invalidArgument`: native arguments are malformed, including platform-layer
  validation failures for `maxLongEdge` or `quality`.
- `openFailed`: FFmpeg could not open the local video file.
- `noVideoStream`: the input file does not contain a video stream.
- `readInfoFailed`: metadata probing failed.
- `decodeFailed`: a frame could not be decoded.
- `outputFailed`: cover file creation, scaling, or rotation failed.
- `ffmpegUnavailable`: bundled native FFmpeg libraries could not be loaded.
- `unknown`: fallback for an unrecognized platform error code.

```dart
try {
  final coverPath = await LgplFfmpegFlutter.generateCover(
    videoPath: video.path,
  );
  if (coverPath != null) {
    // The path points to a generated .png file in the platform cache directory.
  }
} on VideoProcessException catch (error) {
  // Handle error.code and error.message in the host app.
}
```

## Example App

The `example` app lets you pick a local video, read its metadata, generate a
cover frame, and preview the generated image path.

```sh
cd example
flutter run
```

On Android, run the example on an `arm64-v8a` device or emulator unless you
have rebuilt and bundled FFmpeg libraries for another ABI.

## Bundled FFmpeg

This package bundles prebuilt FFmpeg 8.0.1 dynamic libraries for Android and
iOS. The bundled libraries are built with an LGPL-oriented configuration:

```text
--disable-gpl
--disable-nonfree
--enable-shared
--disable-static
--disable-programs
--disable-doc
--disable-network
--disable-autodetect
```

Forbidden options for this package include `--enable-gpl`,
`--enable-nonfree`, `--enable-libx264`, `--enable-libx265`,
`--enable-libxvid`, and `--enable-libvidstab`.

The build script and recorded configuration are included in the package:

- `scripts/build_ffmpeg_lgpl.sh`
- `third_party/ffmpeg/VERSION`
- `third_party/ffmpeg/source-url.txt`
- `third_party/ffmpeg/configure_args.txt`

## License

The `lgpl_ffmpeg_flutter` plugin code is released under the MIT License.

The bundled FFmpeg dynamic libraries are distributed under the GNU Lesser
General Public License version 2.1 or later (LGPL v2.1+). FFmpeg license texts
and third-party notices are included in `NOTICE.md` and
`third_party/ffmpeg/LICENSES/`.

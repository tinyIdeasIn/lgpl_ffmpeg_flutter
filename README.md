# lgpl_ffmpeg_flutter

[![pub package](https://img.shields.io/badge/pub-0.1.1-blue.svg)](https://pub.dev/packages/lgpl_ffmpeg_flutter)

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

## Quick Start

```dart
import 'package:lgpl_ffmpeg_flutter/lgpl_ffmpeg_flutter.dart';

final info = await LgplFfmpegFlutter.readInfo(videoPath: video.path);
final backend = await LgplFfmpegFlutter.backendInfo();

final cover = await LgplFfmpegFlutter.generateCoverImage(
  videoPath: video.path,
  preferredTimes: const [Duration(seconds: 1), Duration(seconds: 3)],
  maxLongEdge: 1280,
  quality: 90,
);

final frame = await LgplFfmpegFlutter.extractFrame(
  videoPath: video.path,
  time: const Duration(seconds: 5),
  maxLongEdge: 720,
);

final deletedCount = await LgplFfmpegFlutter.deleteGeneratedFiles();

print(info.duration);
print(info.videoCodec);
print(backend.license);
print(cover?.path);
print(frame?.actualTime);
print(deletedCount);
```

## Reading Video Info

`readInfo` returns a `VideoInfo` value with the following fields:

- `duration`: video duration.
- `width` and `height`: encoded video dimensions when available.
- `rotation`: display rotation in degrees, defaulting to `0`.
- `bitrate`: stream bitrate when available.
- `mimeType`: container or video MIME type when available.
- `formatName`: FFmpeg container format name when available.
- `videoCodec`: FFmpeg video codec name when available.
- `audioCodec`: FFmpeg audio codec name when available.
- `fileSizeBytes`: local file size when available.

```dart
final info = await LgplFfmpegFlutter.readInfo(videoPath: video.path);

debugPrint('duration=${info.duration}');
debugPrint('size=${info.width}x${info.height}');
debugPrint('codec=${info.videoCodec}/${info.audioCodec}');
```

## Backend Diagnostics

`backendInfo` returns runtime diagnostics from the bundled FFmpeg libraries:

- `ffmpegVersion`: FFmpeg version string compiled into libavutil.
- `avformatVersion`, `avcodecVersion`, and `avutilVersion`: linked library
  versions.
- `configuration`: FFmpeg configure arguments compiled into the libraries.
- `license`: FFmpeg license string reported by libavformat.
- `supportedInputFormats`: container formats enabled by the bundled build.
- `supportedVideoDecoders`: video decoders enabled by the bundled build.
- `supportedAudioDecoders`: audio decoders enabled by the bundled build.
- `outputImageFormat`: generated image format, currently `png`.

```dart
final backend = await LgplFfmpegFlutter.backendInfo();

debugPrint(backend.ffmpegVersion);
debugPrint(backend.supportedInputFormats.join(', '));
debugPrint(backend.configuration);
```

## Cover And Frame Extraction

`generateCoverImage` writes a `.png` image file in the platform cache directory
and returns a `CoverImage` value with the generated path, output dimensions,
requested timestamp, and best-effort decoded frame timestamp.

`extractFrame` extracts a frame near the requested time and returns the same
`CoverImage` metadata. Seeking is best-effort and does not guarantee exact
frame-accurate output.

`generateCover` remains available for backwards compatibility and returns only
the generated PNG path.

```dart
final cover = await LgplFfmpegFlutter.generateCoverImage(
  videoPath: video.path,
  preferredTimes: const [Duration(milliseconds: 300), Duration(seconds: 1)],
  maxLongEdge: 1280,
);

final frame = await LgplFfmpegFlutter.extractFrame(
  videoPath: video.path,
  time: const Duration(seconds: 5),
  maxLongEdge: 720,
);

debugPrint(cover?.path);
debugPrint('${frame?.width} x ${frame?.height}');
```

`generateCoverImage` accepts these options:

- `preferredTimes`: candidate timestamps to try in order. When omitted, the
  native implementation chooses fallback timestamps.
- `maxLongEdge`: maximum output width or height after scaling. The value must
  be greater than `0`; the default is `1920`.
- `quality`: reserved output quality value from `1` to `100`; the default is
  `95`.

`deleteGeneratedFiles` removes only plugin-generated `lgpl_ffmpeg_*.png` files
from the platform cache and returns the number of deleted files.

```dart
final deletedCount = await LgplFfmpegFlutter.deleteGeneratedFiles();
debugPrint('Deleted $deletedCount generated image(s).');
```

## Error Handling

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
  final cover = await LgplFfmpegFlutter.generateCoverImage(
    videoPath: video.path,
  );
  if (cover != null) {
    // The path points to a generated .png file in the platform cache directory.
  }
} on VideoProcessException catch (error) {
  // Handle error.code and error.message in the host app.
}
```

## Example App

The `example` app lets you pick a local video, read its metadata, generate a
cover frame, extract a frame near a chosen timestamp, inspect backend
capabilities, and clean generated cache files.

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

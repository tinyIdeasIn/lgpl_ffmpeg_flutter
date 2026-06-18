# lgpl_ffmpeg_flutter

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
print(cover?.coverPath);
```

`generateCover` writes a `.ppm` image file in the platform cache directory.
Convert that file in the host app if JPEG or PNG output is required.

`generateCover` returns `null` when the video can be opened but no candidate
time produces a frame. If native FFmpeg libraries are unavailable, the plugin
throws a structured `ffmpegUnavailable` error.

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

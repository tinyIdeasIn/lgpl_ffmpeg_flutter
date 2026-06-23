## 0.1.2

* Added HDR-aware cover generation on Android and iOS with PQ and HLG tone
  mapping to SDR PNG output.
* Improved BT.2020 HDR cover color conversion before writing generated cover
  images.
* Preserved the existing cover generation API and structured result fields for
  SDR and HDR videos.

## 0.1.1

* Expanded README examples for metadata reading, backend diagnostics, cover
  extraction, frame extraction, cache cleanup, and error handling.
* Improved the example app layout with clearer action controls, generated image
  summaries, backend capability summaries, and configurable frame extraction
  time.

## 0.1.0

* Added structured `CoverImage` results through `generateCoverImage`.
* Added `extractFrame` for best-effort PNG frame extraction near a requested
  timestamp.
* Added `deleteGeneratedFiles` for removing plugin-generated cache files.
* Expanded `backendInfo` with bundled input format, decoder, and output image
  capability details.

## 0.0.4

* Added `backendInfo` for inspecting bundled FFmpeg version, configuration, and
  license details at runtime.
* Expanded `VideoInfo` with optional format, codec, and file size metadata.
* Updated the example app to display the new metadata and backend diagnostics.

## 0.0.3

* Fixed pub.dev package scoring items by documenting the current release in the
  changelog.
* Improved public API dartdoc coverage for the plugin entry point, platform
  interface, video metadata, and error types.

## 0.0.2

* Added bundled LGPL FFmpeg binaries and framework metadata for Android and
  iOS.
* Improved package documentation and public API dartdoc coverage.
* Kept the plugin API limited to video metadata reading and cover extraction.

## 0.0.1

* Initial controlled Flutter plugin scaffold for LGPL FFmpeg video info reading
  and cover extraction.
* Added iOS and Android MethodChannel shells without arbitrary FFmpeg command
  execution.
* Added LGPL FFmpeg compliance placeholders.

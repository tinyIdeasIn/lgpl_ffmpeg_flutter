# iOS LGPL FFmpeg Frameworks

Place the approved LGPL FFmpeg `xcframework` bundle here before production use.

Expected layout after running `scripts/build_ffmpeg_lgpl.sh`:

```text
ios/Frameworks/
  libavcodec.xcframework/
  libavformat.xcframework/
  libavutil.xcframework/
  libswscale.xcframework/
  libswresample.xcframework/
```

The framework must be built with shared-library LGPL settings and must not include
GPL or nonfree codecs.

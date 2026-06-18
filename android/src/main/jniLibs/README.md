# Android LGPL FFmpeg Dynamic Libraries

Place approved LGPL FFmpeg shared libraries here before production use.

Expected ABI layout:

```text
android/src/main/jniLibs/
  arm64-v8a/
    libavcodec.so
    libavformat.so
    libavutil.so
    libswscale.so
    libswresample.so
```

Do not place GPL or nonfree builds in this directory.

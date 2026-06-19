/// Runtime information reported by the bundled LGPL FFmpeg backend.
class FfmpegBackendInfo {
  /// Creates backend diagnostic information.
  const FfmpegBackendInfo({
    required this.ffmpegVersion,
    required this.avformatVersion,
    required this.avcodecVersion,
    required this.avutilVersion,
    required this.configuration,
    required this.license,
  });

  /// Converts a native method-channel result map into [FfmpegBackendInfo].
  factory FfmpegBackendInfo.fromMap(Map<Object?, Object?> map) {
    return FfmpegBackendInfo(
      ffmpegVersion: map['ffmpegVersion'] as String? ?? '',
      avformatVersion: map['avformatVersion'] as String? ?? '',
      avcodecVersion: map['avcodecVersion'] as String? ?? '',
      avutilVersion: map['avutilVersion'] as String? ?? '',
      configuration: map['configuration'] as String? ?? '',
      license: map['license'] as String? ?? '',
    );
  }

  /// FFmpeg version string compiled into libavutil.
  final String ffmpegVersion;

  /// Runtime libavformat version.
  final String avformatVersion;

  /// Runtime libavcodec version.
  final String avcodecVersion;

  /// Runtime libavutil version.
  final String avutilVersion;

  /// FFmpeg configure arguments compiled into the bundled libraries.
  final String configuration;

  /// FFmpeg license string reported by libavformat.
  final String license;
}

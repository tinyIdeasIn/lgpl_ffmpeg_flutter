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
    this.supportedInputFormats = const <String>[],
    this.supportedVideoDecoders = const <String>[],
    this.supportedAudioDecoders = const <String>[],
    this.outputImageFormat = 'png',
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
      supportedInputFormats: _stringList(map['supportedInputFormats']),
      supportedVideoDecoders: _stringList(map['supportedVideoDecoders']),
      supportedAudioDecoders: _stringList(map['supportedAudioDecoders']),
      outputImageFormat: map['outputImageFormat'] as String? ?? 'png',
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

  /// Input container formats enabled in the bundled build.
  final List<String> supportedInputFormats;

  /// Video decoders enabled in the bundled build.
  final List<String> supportedVideoDecoders;

  /// Audio decoders enabled in the bundled build.
  final List<String> supportedAudioDecoders;

  /// Generated image format.
  final String outputImageFormat;

  static List<String> _stringList(Object? value) {
    if (value is! List) {
      return const <String>[];
    }
    return value.whereType<String>().toList(growable: false);
  }
}

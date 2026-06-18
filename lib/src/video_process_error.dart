import 'package:flutter/services.dart';

/// Error codes returned by the native video processing backend.
enum VideoProcessErrorCode {
  /// The requested video path or argument was invalid.
  invalidPath,

  /// The native backend could not open the video.
  openFailed,

  /// The file did not contain a readable video stream.
  noVideoStream,

  /// Metadata extraction failed.
  readInfoFailed,

  /// Frame decoding failed.
  decodeFailed,

  /// Writing the generated output failed.
  outputFailed,

  /// The bundled FFmpeg backend was unavailable.
  ffmpegUnavailable,

  /// The native backend rejected an argument.
  invalidArgument,

  /// An unmapped native error code was returned.
  unknown;

  /// Converts a native error [code] into a typed enum value.
  static VideoProcessErrorCode fromCode(String code) {
    for (final value in values) {
      if (value.name == code) {
        return value;
      }
    }
    return unknown;
  }
}

/// Exception thrown when video metadata or cover extraction fails.
class VideoProcessException implements Exception {
  /// Creates a video processing exception.
  const VideoProcessException({
    required this.code,
    required this.message,
    this.details,
  });

  /// Converts a platform-channel exception into [VideoProcessException].
  factory VideoProcessException.fromPlatformException(
    PlatformException exception,
  ) {
    return VideoProcessException(
      code: VideoProcessErrorCode.fromCode(exception.code),
      message: exception.message ?? exception.code,
      details: exception.details,
    );
  }

  /// Machine-readable failure code.
  final VideoProcessErrorCode code;

  /// Human-readable failure message.
  final String message;

  /// Optional native backend details.
  final Object? details;

  /// Returns a concise diagnostic string.
  @override
  String toString() {
    return 'VideoProcessException(${code.name}, $message)';
  }
}

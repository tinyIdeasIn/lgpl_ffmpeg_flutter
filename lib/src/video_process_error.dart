import 'package:flutter/services.dart';

enum VideoProcessErrorCode {
  invalidPath,
  openFailed,
  noVideoStream,
  readInfoFailed,
  decodeFailed,
  outputFailed,
  ffmpegUnavailable,
  invalidArgument,
  unknown;

  static VideoProcessErrorCode fromCode(String code) {
    for (final value in values) {
      if (value.name == code) {
        return value;
      }
    }
    return unknown;
  }
}

class VideoProcessException implements Exception {
  const VideoProcessException({
    required this.code,
    required this.message,
    this.details,
  });

  factory VideoProcessException.fromPlatformException(
    PlatformException exception,
  ) {
    return VideoProcessException(
      code: VideoProcessErrorCode.fromCode(exception.code),
      message: exception.message ?? exception.code,
      details: exception.details,
    );
  }

  final VideoProcessErrorCode code;
  final String message;
  final Object? details;

  @override
  String toString() {
    return 'VideoProcessException(${code.name}, $message)';
  }
}

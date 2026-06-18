/// Basic metadata for a video file.
class VideoInfo {
  /// Creates video metadata returned by the native backend.
  const VideoInfo({
    required this.duration,
    this.width,
    this.height,
    this.rotation = 0,
    this.bitrate,
    this.mimeType = '',
  });

  /// Converts a native method-channel result map into [VideoInfo].
  factory VideoInfo.fromMap(Map<Object?, Object?> map) {
    final durationMs = map['durationMs'];
    if (durationMs is! int) {
      throw const FormatException('Missing durationMs in video info result.');
    }

    return VideoInfo(
      duration: Duration(milliseconds: durationMs),
      width: map['width'] as int?,
      height: map['height'] as int?,
      rotation: map['rotation'] as int? ?? 0,
      bitrate: map['bitrate'] as int?,
      mimeType: map['mimeType'] as String? ?? '',
    );
  }

  /// Playback duration of the video.
  final Duration duration;

  /// Encoded video width in pixels, when available.
  final int? width;

  /// Encoded video height in pixels, when available.
  final int? height;

  /// Rotation metadata in degrees.
  final int rotation;

  /// Video bitrate in bits per second, when available.
  final int? bitrate;

  /// Container or stream MIME type reported by the native backend.
  final String mimeType;
}

class VideoInfo {
  const VideoInfo({
    required this.duration,
    this.width,
    this.height,
    this.rotation = 0,
    this.bitrate,
    this.mimeType = '',
  });

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

  final Duration duration;
  final int? width;
  final int? height;
  final int rotation;
  final int? bitrate;
  final String mimeType;
}

/// Metadata for a generated PNG image.
class CoverImage {
  /// Creates metadata for a generated cover or extracted frame.
  const CoverImage({
    required this.path,
    required this.width,
    required this.height,
    required this.requestedTime,
    this.actualTime,
  });

  /// Converts a native method-channel result map into [CoverImage].
  factory CoverImage.fromMap(Map<Object?, Object?> map) {
    final path = map['coverPath'];
    final width = map['width'];
    final height = map['height'];
    final requestedTimeMs = map['requestedTimeMs'];
    if (path is! String || path.isEmpty) {
      throw const FormatException('Missing coverPath in cover image result.');
    }
    if (width is! int || height is! int) {
      throw const FormatException('Missing dimensions in cover image result.');
    }
    if (requestedTimeMs is! int) {
      throw const FormatException(
        'Missing requestedTimeMs in cover image result.',
      );
    }

    final actualTimeMs = map['actualTimeMs'];
    return CoverImage(
      path: path,
      width: width,
      height: height,
      requestedTime: Duration(milliseconds: requestedTimeMs),
      actualTime: actualTimeMs is int
          ? Duration(milliseconds: actualTimeMs)
          : null,
    );
  }

  /// Generated PNG path in the platform cache directory.
  final String path;

  /// Output image width in pixels.
  final int width;

  /// Output image height in pixels.
  final int height;

  /// Candidate timestamp requested by the caller.
  final Duration requestedTime;

  /// Best-effort decoded frame timestamp, when available.
  final Duration? actualTime;
}

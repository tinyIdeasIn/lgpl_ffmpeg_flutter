import 'dart:io';

import 'package:file_picker/file_picker.dart';
import 'package:flutter/material.dart';
import 'package:lgpl_ffmpeg_flutter/lgpl_ffmpeg_flutter.dart';

void main() {
  runApp(const ExampleApp());
}

class ExampleApp extends StatelessWidget {
  const ExampleApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      theme: ThemeData(
        colorScheme: ColorScheme.fromSeed(seedColor: Colors.teal),
      ),
      home: const VideoProbePage(),
    );
  }
}

class VideoProbePage extends StatefulWidget {
  const VideoProbePage({super.key});

  @override
  State<VideoProbePage> createState() => _VideoProbePageState();
}

class _VideoProbePageState extends State<VideoProbePage> {
  String? _videoPath;
  VideoInfo? _info;
  FfmpegBackendInfo? _backendInfo;
  CoverImage? _cover;
  CoverImage? _frame;
  String? _message;
  String? _backendMessage;
  bool _loading = false;

  @override
  void initState() {
    super.initState();
    _loadBackendInfo();
  }

  Future<void> _loadBackendInfo() async {
    try {
      final info = await LgplFfmpegFlutter.backendInfo();
      if (!mounted) {
        return;
      }
      setState(() {
        _backendInfo = info;
        _backendMessage = null;
      });
    } on VideoProcessException catch (error) {
      if (!mounted) {
        return;
      }
      setState(() {
        _backendMessage = '${error.code.name}: ${error.message}';
      });
    }
  }

  Future<void> _pickAndProcessVideo() async {
    setState(() {
      _loading = true;
      _message = null;
      _info = null;
      _cover = null;
      _frame = null;
    });

    try {
      final result = await FilePicker.platform.pickFiles(
        type: FileType.video,
        allowMultiple: false,
      );
      final path = result?.files.single.path;
      if (path == null) {
        setState(() {
          _message = 'No video selected.';
        });
        return;
      }

      final info = await LgplFfmpegFlutter.readInfo(videoPath: path);
      final cover = await LgplFfmpegFlutter.generateCoverImage(
        videoPath: path,
        preferredTimes: const [Duration(seconds: 1), Duration(seconds: 3)],
        maxLongEdge: 1280,
      );

      setState(() {
        _videoPath = path;
        _info = info;
        _cover = cover;
      });
    } on VideoProcessException catch (error) {
      setState(() {
        _message = '${error.code.name}: ${error.message}';
      });
    } catch (error) {
      setState(() {
        _message = error.toString();
      });
    } finally {
      if (mounted) {
        setState(() {
          _loading = false;
        });
      }
    }
  }

  Future<void> _extractFrame() async {
    final videoPath = _videoPath;
    if (videoPath == null) {
      return;
    }
    setState(() {
      _loading = true;
      _message = null;
    });

    try {
      final frame = await LgplFfmpegFlutter.extractFrame(
        videoPath: videoPath,
        time: const Duration(seconds: 5),
        maxLongEdge: 720,
      );
      setState(() {
        _frame = frame;
      });
    } on VideoProcessException catch (error) {
      setState(() {
        _message = '${error.code.name}: ${error.message}';
      });
    } finally {
      if (mounted) {
        setState(() {
          _loading = false;
        });
      }
    }
  }

  Future<void> _deleteGeneratedFiles() async {
    setState(() {
      _loading = true;
      _message = null;
    });

    try {
      final deletedCount = await LgplFfmpegFlutter.deleteGeneratedFiles();
      setState(() {
        _cover = null;
        _frame = null;
        _message = 'Deleted $deletedCount generated file(s).';
      });
    } on VideoProcessException catch (error) {
      setState(() {
        _message = '${error.code.name}: ${error.message}';
      });
    } finally {
      if (mounted) {
        setState(() {
          _loading = false;
        });
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    final info = _info;
    final backendInfo = _backendInfo;
    return Scaffold(
      appBar: AppBar(title: const Text('lgpl_ffmpeg_flutter')),
      body: ListView(
        padding: const EdgeInsets.all(20),
        children: [
          FilledButton.icon(
            onPressed: _loading ? null : _pickAndProcessVideo,
            icon: _loading
                ? const SizedBox.square(
                    dimension: 18,
                    child: CircularProgressIndicator(strokeWidth: 2),
                  )
                : const Icon(Icons.video_file_outlined),
            label: Text(_loading ? 'Processing...' : 'Pick video'),
          ),
          const SizedBox(height: 8),
          Wrap(
            spacing: 8,
            runSpacing: 8,
            children: [
              OutlinedButton(
                onPressed: _loading || _videoPath == null
                    ? null
                    : _extractFrame,
                child: const Text('Extract frame'),
              ),
              OutlinedButton(
                onPressed: _loading ? null : _deleteGeneratedFiles,
                child: const Text('Clean generated files'),
              ),
            ],
          ),
          const SizedBox(height: 20),
          if (backendInfo != null) ...[
            _InfoRow(label: 'FFmpeg', value: backendInfo.ffmpegVersion),
            _InfoRow(label: 'FFmpeg license', value: backendInfo.license),
            _InfoRow(
              label: 'Input formats',
              value: backendInfo.supportedInputFormats.join(', '),
            ),
            _InfoRow(
              label: 'Video decoders',
              value: backendInfo.supportedVideoDecoders.join(', '),
            ),
            _InfoRow(
              label: 'Audio decoders',
              value: backendInfo.supportedAudioDecoders.join(', '),
            ),
            _InfoRow(
              label: 'Output image',
              value: backendInfo.outputImageFormat,
            ),
            _InfoRow(label: 'FFmpeg config', value: backendInfo.configuration),
            const SizedBox(height: 8),
          ] else if (_backendMessage != null) ...[
            _InfoRow(label: 'FFmpeg backend', value: _backendMessage!),
            const SizedBox(height: 8),
          ],
          if (_message != null) Text(_message!),
          if (_videoPath != null)
            _InfoRow(label: 'Video path', value: _videoPath!),
          if (info != null) ...[
            _InfoRow(label: 'Duration', value: info.duration.toString()),
            _InfoRow(label: 'Size', value: '${info.width} x ${info.height}'),
            _InfoRow(label: 'Rotation', value: '${info.rotation}'),
            _InfoRow(label: 'Bitrate', value: '${info.bitrate ?? '-'}'),
            _InfoRow(label: 'MIME type', value: info.mimeType),
            _InfoRow(label: 'Format', value: info.formatName ?? '-'),
            _InfoRow(label: 'Video codec', value: info.videoCodec ?? '-'),
            _InfoRow(label: 'Audio codec', value: info.audioCodec ?? '-'),
            _InfoRow(
              label: 'File size',
              value: info.fileSizeBytes == null
                  ? '-'
                  : '${info.fileSizeBytes} bytes',
            ),
          ],
          if (_cover != null) ...[
            _CoverPreview(title: 'Cover', image: _cover!),
            const SizedBox(height: 16),
            _InfoRow(label: 'Cover path', value: _cover!.path),
            _InfoRow(
              label: 'Cover size',
              value: '${_cover!.width} x ${_cover!.height}',
            ),
            _InfoRow(
              label: 'Cover time',
              value: _cover!.actualTime?.toString() ?? '-',
            ),
          ],
          if (_frame != null) ...[
            _CoverPreview(title: 'Frame', image: _frame!),
            const SizedBox(height: 16),
            _InfoRow(label: 'Frame path', value: _frame!.path),
            _InfoRow(
              label: 'Frame size',
              value: '${_frame!.width} x ${_frame!.height}',
            ),
            _InfoRow(
              label: 'Frame time',
              value: _frame!.actualTime?.toString() ?? '-',
            ),
          ],
        ],
      ),
    );
  }
}

class _CoverPreview extends StatelessWidget {
  const _CoverPreview({required this.title, required this.image});

  final String title;
  final CoverImage image;

  @override
  Widget build(BuildContext context) {
    final imageFile = File(image.path);
    final colorScheme = Theme.of(context).colorScheme;

    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(title, style: Theme.of(context).textTheme.labelLarge),
        const SizedBox(height: 8),
        ConstrainedBox(
          constraints: const BoxConstraints(maxHeight: 360),
          child: AspectRatio(
            aspectRatio: 16 / 9,
            child: Material(
              clipBehavior: Clip.antiAlias,
              color: colorScheme.surfaceContainerHighest,
              borderRadius: BorderRadius.circular(8),
              child: InkWell(
                onTap: () => _openCoverViewer(context, image.path),
                child: Stack(
                  fit: StackFit.expand,
                  children: [
                    Image.file(
                      imageFile,
                      fit: BoxFit.contain,
                      errorBuilder: (context, error, stackTrace) {
                        return Center(
                          child: Padding(
                            padding: const EdgeInsets.all(20),
                            child: Text(
                              'Unable to load cover image.',
                              style: TextStyle(color: colorScheme.error),
                              textAlign: TextAlign.center,
                            ),
                          ),
                        );
                      },
                    ),
                    Positioned(
                      right: 8,
                      bottom: 8,
                      child: DecoratedBox(
                        decoration: BoxDecoration(
                          color: Colors.black.withValues(alpha: 0.54),
                          borderRadius: BorderRadius.circular(8),
                        ),
                        child: const Padding(
                          padding: EdgeInsets.all(8),
                          child: Icon(
                            Icons.fullscreen,
                            color: Colors.white,
                            size: 22,
                          ),
                        ),
                      ),
                    ),
                  ],
                ),
              ),
            ),
          ),
        ),
      ],
    );
  }

  void _openCoverViewer(BuildContext context, String coverPath) {
    Navigator.of(context).push<void>(
      MaterialPageRoute<void>(
        builder: (_) => _CoverViewerPage(coverPath: coverPath),
      ),
    );
  }
}

class _CoverViewerPage extends StatelessWidget {
  const _CoverViewerPage({required this.coverPath});

  final String coverPath;

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: Colors.black,
      appBar: AppBar(
        backgroundColor: Colors.black,
        foregroundColor: Colors.white,
        title: const Text('Cover'),
      ),
      body: SafeArea(
        child: Center(
          child: InteractiveViewer(
            minScale: 0.5,
            maxScale: 5,
            child: Image.file(
              File(coverPath),
              fit: BoxFit.contain,
              errorBuilder: (context, error, stackTrace) {
                return const Padding(
                  padding: EdgeInsets.all(24),
                  child: Text(
                    'Unable to load cover image.',
                    style: TextStyle(color: Colors.white),
                    textAlign: TextAlign.center,
                  ),
                );
              },
            ),
          ),
        ),
      ),
    );
  }
}

class _InfoRow extends StatelessWidget {
  const _InfoRow({required this.label, required this.value});

  final String label;
  final String value;

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.only(bottom: 12),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(label, style: Theme.of(context).textTheme.labelLarge),
          const SizedBox(height: 4),
          SelectableText(value),
        ],
      ),
    );
  }
}

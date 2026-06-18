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
  String? _coverPath;
  String? _message;
  bool _loading = false;

  Future<void> _pickAndProcessVideo() async {
    setState(() {
      _loading = true;
      _message = null;
      _info = null;
      _coverPath = null;
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
      final coverPath = await LgplFfmpegFlutter.generateCover(
        videoPath: path,
        preferredTimes: const [Duration(seconds: 1), Duration(seconds: 3)],
        maxLongEdge: 1280,
      );

      setState(() {
        _videoPath = path;
        _info = info;
        _coverPath = coverPath;
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

  @override
  Widget build(BuildContext context) {
    final info = _info;
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
          const SizedBox(height: 20),
          if (_message != null) Text(_message!),
          if (_videoPath != null)
            _InfoRow(label: 'Video path', value: _videoPath!),
          if (info != null) ...[
            _InfoRow(label: 'Duration', value: info.duration.toString()),
            _InfoRow(label: 'Size', value: '${info.width} x ${info.height}'),
            _InfoRow(label: 'Rotation', value: '${info.rotation}'),
            _InfoRow(label: 'Bitrate', value: '${info.bitrate ?? '-'}'),
            _InfoRow(label: 'MIME type', value: info.mimeType),
          ],
          if (_coverPath != null)
            _InfoRow(label: 'Cover path', value: _coverPath!),
        ],
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

#ifndef LGPL_FFMPEG_FLUTTER_BACKEND_H_
#define LGPL_FFMPEG_FLUTTER_BACKEND_H_

const char* lgpl_ffmpeg_flutter_backend_status(void);
char* lgpl_ffmpeg_flutter_backend_info(void);
char* lgpl_ffmpeg_flutter_read_info(const char* video_path);
char* lgpl_ffmpeg_flutter_generate_cover(
    const char* video_path,
    const long long* preferred_times_ms,
    int preferred_times_count,
    int max_long_edge,
    int quality,
    const char* cache_dir);
void lgpl_ffmpeg_flutter_free_string(char* value);

#endif

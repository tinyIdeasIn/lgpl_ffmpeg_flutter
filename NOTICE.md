# Third-Party Notices

This package includes prebuilt FFmpeg dynamic libraries.

## FFmpeg

- Version: FFmpeg 8.0.1
- Source: https://ffmpeg.org/releases/ffmpeg-8.0.1.tar.xz
- Project: https://ffmpeg.org/
- License: GNU Lesser General Public License version 2.1 or later (LGPL v2.1+)

The bundled FFmpeg libraries are built as dynamic libraries with an
LGPL-oriented configuration. GPL, nonfree, static linking, FFmpeg programs,
network support, documentation, and autodetected external dependencies are
disabled.

The exact build script and configuration are included in this package:

- `scripts/build_ffmpeg_lgpl.sh`
- `third_party/ffmpeg/VERSION`
- `third_party/ffmpeg/source-url.txt`
- `third_party/ffmpeg/configure_args.txt`

FFmpeg license texts are included in:

- `third_party/ffmpeg/LICENSES/FFmpeg-LICENSE.txt`
- `third_party/ffmpeg/LICENSES/LGPL-2.1.txt`
- `third_party/ffmpeg/LICENSES/LGPL-3.0.txt`

The `lgpl_ffmpeg_flutter` plugin code is distributed under the MIT License.
That MIT License does not change the license terms that apply to FFmpeg.

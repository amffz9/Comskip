# Tests

Tests mirror source responsibilities in config/, detection/, media/, and platform/.

Use Google Test and CTest through CMake (see the root README). Unit tests cover
settings validation and regional profiles, frame rounding, scan-worker lifecycle,
Windows Unicode arguments, checked path formatting, Unicode file operations,
10-bit frame ownership, and ten packed/planar audio representations.

media_smoke generates raw video without external tools. media_formats uses the
ffmpeg executable to generate PCM, AC3, and 10-bit fixtures, then compares serial
and parallel detection output, timestamps, audio volumes, and Unicode paths.
The default vcpkg tests feature supplies ffmpeg and Google Test. Classic-mode
users can supply ffmpeg on PATH or set FFMPEG_EXECUTABLE in CMake. If it is
unavailable, only media_formats is skipped, with a configure-time message.

Use -DCOMSKIP_BUILD_APP=OFF for configuration/worker/platform tests without
FFmpeg linkage; audio/video conversion tests require the application dependencies.
Generated recordings remain in temporary directories and are removed afterward.

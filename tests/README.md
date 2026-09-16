# Tests

Tests mirror source responsibilities in app/, config/, detection/, input/, media/,
output/, platform/, and ui/.

Use Google Test and CTest through CMake (see the root README). Unit tests cover
settings validation and regional profiles, frame rounding, scan-worker lifecycle,
Windows Unicode arguments, checked path formatting, Unicode file operations,
10-bit frame ownership, and ten packed/planar audio representations.

media_smoke generates raw video without external tools. media_formats uses the
ffmpeg executable to generate PCM, AC3, and 10-bit fixtures, then compares serial
and parallel detection output, timestamps, audio volumes, and Unicode paths.
The default vcpkg tests feature supplies ffmpeg and Google Test. Classic-mode
users can supply ffmpeg on PATH or set FFMPEG_EXECUTABLE in CMake. Generated
FFmpeg fixture tests are conditional on that executable being available.

Input tests cover bounded reference/CSV parsing, malformed numbers, released
files, and missing final newlines. Output tests cover XML/plist, timed subtitles,
localized file failures, and safe editable templates. Actual media tests exercise
caption EOF/reopen/failure cleanup, standalone overlapping Unicode subtitles,
CSV subtitle/cutlist roundtrips, damaged streams, and long input filenames.
The short smoke fixture checks one whole-recording commercial interval; it does
not establish detection accuracy for mixed programs and commercial breaks.

Use -DCOMSKIP_BUILD_APP=OFF for configuration/worker/platform tests without
FFmpeg linkage; audio/video conversion tests require the application dependencies.
Generated recordings remain in temporary directories and are removed afterward.

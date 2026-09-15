# Tests

Tests use Google Test and CTest. Configure with CMake and vcpkg (see the root
README), build, then run `ctest --test-dir build --output-on-failure`.
Use `-DCOMSKIP_BUILD_APP=OFF` for unit tests without FFmpeg application linkage.

Commercial-length tests cover frame rounding, strict/optional length tables,
tolerance limits and overrides, minimum-show boundaries, and timing correction.
Worker tests cover frame publication, completion, and repeated startup/shutdown.
Both commercial-length table variants are built separately.

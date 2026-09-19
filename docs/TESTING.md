# Testing

**WSL/Linux with vcpkg**

Keep the Linux source and build directories on WSL's Linux filesystem. Use a
separate Linux vcpkg checkout and binary cache from any Windows installation.
On Ubuntu, install the build prerequisites (and a C++23 compiler such as GCC 14):

```sh
sudo apt-get update
sudo apt-get install -y build-essential g++-14 curl zip unzip tar cmake ninja-build pkg-config autoconf automake libtool nasm
git clone https://github.com/microsoft/vcpkg.git "$HOME/Development/vcpkg"
export VCPKG_ROOT="$HOME/Development/vcpkg"
git -C "$VCPKG_ROOT" checkout 908da3a305a0a8028d9602ab241b433652b3df69
"$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics
```

The checkout above matches `vcpkg.json` and CI. Skip the clone when vcpkg is
already installed. From the Comskip source directory, configure a fresh build
directory with the toolchain; do not reuse a cache configured without vcpkg:

```sh
VCPKG_MAX_CONCURRENCY=6 cmake -S . -B build/wsl-release -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DCMAKE_CXX_COMPILER=g++-14 -DCMAKE_BUILD_TYPE=Release
cmake --build build/wsl-release --parallel 6
cmake --build build/wsl-release --target comskip-check
```

Configuration installs the manifest dependencies, including FFmpeg and Google
Test, and reuses vcpkg's binary cache on subsequent builds. The executable is
`build/wsl-release/comskip`; test results are in
`build/wsl-release/test-results.log`. Reduce parallelism on smaller machines.
Set `VCPKG_ROOT` again in a new shell, or add the export to your shell profile.

**GCC 15 and argtable2 2.13**

GCC 15 defaults C compilation to C23. The argtable2 2.13 port pinned by this
project includes a legacy `getopt.c` that is not C23-compatible and can fail
with:

```text
error: too many arguments to function 'getenv'
```

This is an argtable2 build failure rather than a Comskip C++23 failure. On a
GCC 15 host, use a custom vcpkg triplet that builds C sources as GNU C17, for
example by adding the following to the triplet used for the manifest install:

```cmake
set(VCPKG_C_FLAGS "-std=gnu17")
```

Keep the rest of the triplet appropriate for the target platform. This is
especially relevant to Alpine 3.24 builds, whose GCC selects C23 by default.
Remove the workaround after the pinned argtable2 port is updated or patched to
compile as C23.

When a vcpkg port fails during CMake configuration, CMake may subsequently
report that it is unable to find Ninja or that `CMAKE_CXX_COMPILER` is not set.
Those are often follow-on messages rather than the cause. Inspect the first
failed port's log under:

```text
buildtrees/<port>/install-*-out.log
```

Resolve the earliest compiler or port error there before troubleshooting the
later CMake diagnostics.

**Windows testing**

Use the CMake test target instead of launching test executables individually:

```powershell
$env:SDL_VIDEODRIVER = "dummy"
$env:SDL_AUDIODRIVER = "dummy"
$env:ASAN_OPTIONS = "abort_on_error=1:halt_on_error=1:log_path=asan"
$env:UBSAN_OPTIONS = "halt_on_error=1:print_stacktrace=1:log_path=ubsan"
cmake --build bin/build23-sanitize --target comskip-check -j 3
```

`comskip-check` now depends on the test executables, so a fresh sanitizer
configuration builds them before CTest runs. CMake also applies the dummy SDL
drivers and sanitizer variables to every registered test. CTest writes the
test summary to `bin/<build>/test-results.log`; sanitizer reports are written
beside it with the `asan` or `ubsan` prefix when a finding occurs.

Keep sanitizer runs noninteractive. Always set `ASAN_OPTIONS` with
`abort_on_error=1`, `halt_on_error=1`, and `log_path=asan`, and set
`UBSAN_OPTIONS` with `halt_on_error=1`, `print_stacktrace=1`, and
`log_path=ubsan`. Run `cmake --build ... --target comskip-check` instead of
launching a test executable directly; this keeps missing DLL and sanitizer
failures in the CTest log instead of opening Windows error dialogs. If a
dialog still appears, record the executable and missing runtime in
`bin/<build>/test-results.log` and `docs/BUGS.md` before changing the build.

When running manually, set the same variables in the shell before starting
Comskip or a test. Do not dismiss an ASan or missing-DLL dialog and continue
without a record: copy the executable name, error text, and the corresponding
`asan.*`/`ubsan.*` log into the issue notes. The reliable dialog-suppression
recipe is therefore:

```powershell
$env:ASAN_OPTIONS = "abort_on_error=1:halt_on_error=1:log_path=asan"
$env:UBSAN_OPTIONS = "halt_on_error=1:print_stacktrace=1:log_path=ubsan"
cmake --build bin/build23-sanitize --target comskip-check -j 3
```

Keep this recipe in mind for every future Windows sanitizer run. CTest is the
supported entry point because it supplies copied runtime DLLs and captures the
failure in `test-results.log` instead of relying on interactive Windows error
dialogs.

The current Windows Clang ASan runtime still aborts with `0xc0000005` in a
small set of expected-failure/unwind paths before Comskip can construct its
typed diagnostic. This is tracked as B110 in `docs/BUGS.md`; it is a runtime
limitation, not a reason to disable sanitizer reporting or hide failures.

The Windows CMake setup copies the vcpkg GoogleTest runtime DLLs beside the
tests. This prevents the missing-`gtest_main.dll` loader dialog that occurs
when a test executable is launched without the vcpkg runtime directory on
`PATH`. Keep test execution under CTest so failures remain in the log. If a
loader dialog appears anyway, stop the run, record the executable and missing
DLL in `docs/BUGS.md`, and rerun through `comskip-check`; do not repeatedly
dismiss the dialog or launch the test binary directly.

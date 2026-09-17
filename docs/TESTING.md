# Windows testing

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
